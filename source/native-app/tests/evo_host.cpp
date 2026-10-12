// Real EVO decoder/presentation integration regression tests.
#include "evo_backend.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dovi_meta.h>
}
// Production logging is tested by the app; this test has no account/network state.
void dlog(const char*, ...) {}

static void decode(const char* path, bool ten_bit) {
    AVFormatContext* input = nullptr;
    assert(avformat_open_input(&input, path, nullptr, nullptr) == 0);
    assert(avformat_find_stream_info(input, nullptr) >= 0);
    int stream = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    assert(stream >= 0);
    std::string error;
    auto decoder = EvoDecoder::open(input->streams[stream]->codecpar, error);
    assert(decoder && error.empty() && !decoder->hardware());
    AVPacket* packet = av_packet_alloc();
    int pictures = 0;
    int64_t previous = INT64_MIN;
    VideoFrame held;
    std::vector<uint8_t> saved;
    auto drain = [&] {
        int received = 0;
        for (;;) {
            pp_frame frame{};
            int r = decoder->receive(frame);
            assert(r >= 0);
            if (!r) return received;
            assert(frame.width == 192 && frame.height == 108);
            assert(frame.pts_us != INT64_MIN && frame.pts_us > previous);
            previous = frame.pts_us;
            VideoFrame owned;
            owned.w = frame.width; owned.h = frame.height;
            assert(decoder->pack(frame, owned));
            assert(owned.format == (ten_bit ? VideoFormat::Nv12_10 : VideoFormat::Nv12));
            if (!pictures) { held = owned; saved = held.pixels; }
            assert(held.pixels == saved); // decoding never mutates owned GL input
            if (ten_bit) {
                uint16_t value; std::memcpy(&value, owned.pixels.data() + 100, 2);
                assert(value > 0 && value <= 1023);
            }
            pictures++; received++;
        }
    };
    for (int pass = 0; pass < 2; pass++) {
        pictures = 0; previous = INT64_MIN;
        while (av_read_frame(input, packet) >= 0) {
            if (packet->stream_index == stream) {
                int64_t pts = av_rescale_q(packet->pts, input->streams[stream]->time_base, AVRational{1,1000000});
                int sent;
                while ((sent = decoder->send(packet->data, packet->size, pts)) > 0) assert(drain() > 0);
                assert(sent == 0); drain();
            }
            av_packet_unref(packet);
        }
        int sent;
        while ((sent = decoder->send(nullptr, 0, INT64_MIN)) > 0) assert(drain() > 0);
        assert(sent == 0); drain();
        assert(pictures == 24); // includes delayed B pictures at EOF
        if (!pass) { decoder->flush(); assert(av_seek_frame(input, stream, 0, AVSEEK_FLAG_BACKWARD) >= 0); }
    }
    av_packet_free(&packet); avformat_close_input(&input);
}

int main(int argc, char** argv) {
    assert(argc == 3);
    decode(argv[1], false); decode(argv[2], true);
    // Audio master: at 120 Hz every 24 fps source picture is held for five
    // display ticks. An early next picture must never block the render thread.
    VideoFrame owned;
    owned.w = 192; owned.h = 108; owned.format = VideoFormat::Nv12;
    owned.pixels.assign(192 * 108 * 3 / 2, 128);
    EvoPresenter presenter;
    for (int picture = 0; picture < 48; picture++) {
        double pts = picture / 24.0;
        assert(presenter.publish(owned.picture(), pts, pts) == 0);
        for (int tick = 1; tick < 5; tick++)
            assert(presenter.publish(owned.picture(), (picture + 1) / 24.0, pts + tick / 120.0) == 2);
    }
    assert(presenter.publish(owned.picture(), 1.0, 3.0) == 1); // truly late
    presenter.reset();
    assert(presenter.publish(owned.picture(), 122.0, 122.0) == 0); // seek reset
    assert(presenter.publish(owned.picture(), 122.0 + 1.0/24, 122.0) == 2); // held clock
    AVCodecParameters* par = avcodec_parameters_alloc();
    par->codec_id = AV_CODEC_ID_HEVC; par->width = 192; par->height = 108;
    auto* side = av_packet_side_data_new(&par->coded_side_data, &par->nb_coded_side_data,
        AV_PKT_DATA_DOVI_CONF, sizeof(AVDOVIDecoderConfigurationRecord), 0);
    assert(side);
    auto* dovi = reinterpret_cast<AVDOVIDecoderConfigurationRecord*>(side->data);
    std::memset(dovi, 0, sizeof(*dovi)); dovi->dv_profile = 5;
    std::string error;
    assert(!EvoDecoder::open(par, error) && error.find("Profile 5") != std::string::npos);
    avcodec_parameters_free(&par);
    std::puts("EVO integration: SDR/10-bit B-frame decode, EOF, reopen after seek, owned planes, 24/120 cadence, early/late frames and P5 rejection passed");
}
