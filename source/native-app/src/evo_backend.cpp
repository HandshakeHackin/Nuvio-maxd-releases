// SPDX-License-Identifier: GPL-3.0-or-later
#include "evo_backend.h"
#include "evo_dovi.h"
#include "util.h"
#include <cmath>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

std::unique_ptr<EvoDecoder> EvoDecoder::open(const AVCodecParameters* par, std::string& error) {
    if (!par || !video_dimensions_valid(par->width, par->height)) {
        error = "Unsupported video dimensions"; return nullptr;
    }
    if (evo_dovi_stream_profile(par) == 5) {
        error = "Dolby Vision Profile 5 needs a colour reshaper; choose an HDR10 or SDR stream.";
        return nullptr;
    }
    evo_vdec_open_params p{};
    p.backend = EVO_VDEC_BACKEND_NATIVE;
    p.codec_id = par->codec_id; p.width = par->width; p.height = par->height;
    p.extradata = par->extradata; p.extradata_size = par->extradata_size;
    p.avctx_params = const_cast<AVCodecParameters*>(par);
    p.thread_count = 4; p.thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    p.skip_loop_filter = p.skip_frame = p.skip_idct = EVO_VDEC_KEEP;
    evo_vdec_prefer_nv12(1);
    auto out = std::unique_ptr<EvoDecoder>(new EvoDecoder);
    out->decoder_ = evo_vdec_open(&p, nullptr);
    if (!out->decoder_) {
        error = std::string("EVO video decoder could not open: ") +
                evo_vdec_open_result_name(evo_vdec_last_open_result());
        return nullptr;
    }
    dlog("player: EVO Player 0.13.0 video engine (2db44b52), %s decoding",
         out->hardware() ? "hardware" : "software");
    return out;
}
EvoDecoder::~EvoDecoder() {
    if (decoder_) {
        evo_vdec_stats s{}; evo_vdec_get_stats(decoder_, &s);
        dlog("evo: closed after %llu pictures, %llu errors; decode p95 %llu us",
             (unsigned long long)s.frames_out, (unsigned long long)s.fatal_errors,
             (unsigned long long)evo_vdec_decode_p95_us(decoder_));
        evo_vdec_close(decoder_);
    }
    if (scaler_) sws_freeContext(scaler_);
}
int EvoDecoder::send(const uint8_t* data, int size, int64_t pts) {
    return evo_vdec_send(decoder_, data, size, pts);
}
int EvoDecoder::receive(pp_frame& frame) {
    frame = {};
    int r = evo_vdec_receive(decoder_, &frame);
    if (r == 2) {
        auto* raw = static_cast<AVFrame*>(evo_vdec_ffmpeg_avframe(decoder_));
        if (!raw) return -1;
        frame.width = raw->width; frame.height = raw->height;
        frame.pts_us = raw->best_effort_timestamp == AV_NOPTS_VALUE ? INT64_MIN : raw->best_effort_timestamp;
        frame.format = PP_FRAME_RGBA;
    }
    return r;
}
bool EvoDecoder::pack(const pp_frame& frame, VideoFrame& out) {
    auto* raw = static_cast<AVFrame*>(evo_vdec_ffmpeg_avframe(decoder_));
    if (raw && raw->color_range != AVCOL_RANGE_UNSPECIFIED)
        out.full_range = raw->color_range == AVCOL_RANGE_JPEG;
    if (raw && raw->format == AV_PIX_FMT_P010LE) {
        YuvPicture y{};
        y.y = raw->data[0]; y.u = raw->data[1];
        y.y_stride = raw->linesize[0]; y.c_stride = raw->linesize[1];
        y.width = raw->width; y.height = raw->height;
        y.interleaved = true; y.wide = true; y.shift = 6;
        return pack_video_yuv(y, out);
    }
    if (frame.format == PP_FRAME_RGBA) {
        if (!raw) return false;
        scaler_ = sws_getCachedContext(scaler_, raw->width, raw->height,
            static_cast<AVPixelFormat>(raw->format), raw->width, raw->height,
            AV_PIX_FMT_RGBA, SWS_POINT, nullptr, nullptr, nullptr);
        if (!scaler_) return false;
        out.format = VideoFormat::Rgba;
        out.pixels.resize(size_t(out.w) * out.h * 4);
        uint8_t* dst[4] = {out.pixels.data(), nullptr, nullptr, nullptr};
        int strides[4] = {out.w * 4, 0, 0, 0};
        return sws_scale(scaler_, raw->data, raw->linesize, 0, raw->height, dst, strides) == out.h;
    }
    YuvPicture y{};
    y.y = frame.planes[0]; y.u = frame.planes[1]; y.v = frame.planes[2];
    y.y_stride = frame.strides[0]; y.c_stride = frame.strides[1]; y.v_stride = frame.strides[2];
    y.width = frame.width; y.height = frame.height;
    y.interleaved = frame.format == PP_FRAME_NV12 || frame.format == PP_FRAME_NV12_10;
    y.wide = frame.format == PP_FRAME_NV12_10 || frame.format == PP_FRAME_YUV420P10;
    if (hardware() && y.wide && sample_shift_ < 0) sample_shift_ = video_sample_shift(y);
    y.shift = hardware() && y.wide ? sample_shift_ : 0;
    return pack_video_yuv(y, out);
}
void EvoDecoder::flush() { evo_vdec_flush(decoder_); sample_shift_ = -1; }
bool EvoDecoder::hardware() const { return evo_vdec_active(decoder_) == EVO_VDEC_BACKEND_NATIVE; }

EvoPresenter::EvoPresenter() {
    pp_playback_init(&playback_);
    pp_playback_set_clock_source(&playback_, media_clock, this);
    reset();
}
EvoPresenter::~EvoPresenter() { pp_playback_shutdown(&playback_); }
void EvoPresenter::reset() { pp_playback_on_file_open(&playback_); }
int64_t EvoPresenter::media_clock(void* self) { return static_cast<EvoPresenter*>(self)->media_us_; }
int EvoPresenter::publish(const VideoPicture& p, double pts, double media_time) {
    if (!std::isfinite(pts) || !std::isfinite(media_time)) return -1;
    media_us_ = int64_t(media_time * 1000000);
    // The UI never submits an early frame: do not let a pacing sleep block
    // controller processing. Pause/buffer holds are handled before this call.
    if (pts > media_time && pp_playback_has_display(&playback_)) return 2;
    if (p.format == VideoFormat::Rgba) return 0; // exotic software format
    pp_frame f{};
    f.format = p.format == VideoFormat::Nv12_10 ? PP_FRAME_NV12_10 : PP_FRAME_NV12;
    f.width = p.w; f.height = p.h; f.coded_height = p.h;
    f.planes[0] = p.y; f.planes[1] = p.uv;
    const int bytes = p.format == VideoFormat::Nv12_10 ? 2 : 1;
    f.strides[0] = p.w * bytes; f.strides[1] = ((p.w + 1) / 2) * 2 * bytes;
    f.pts_us = int64_t(pts * 1000000);
    f.color_trc = p.colors == YuvColors::Hdr10 ? AVCOL_TRC_SMPTE2084 : AVCOL_TRC_BT709;
    return pp_playback_push_frame(&playback_, &f);
}
