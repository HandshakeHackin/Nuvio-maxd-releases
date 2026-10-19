// libVLC owns demux, decoding and scheduling. Pictures are copied into owned
// YUV frames for the existing OpenGL renderer; console audio remains PCM.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "vlc_backend.h"
#include "pcm_out.h"
#include "util.h"
#include "vlc_colour.h"
#include "vlc_cancel.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#ifdef NUVIO_HAVE_VLC
#include "../third_party/json.hpp"
#include <vlc/vlc.h>
#ifdef NUVIO_VLC_STATIC
#include "hwdec_ps5.h"
extern "C" void nuvio_vlc_static_anchor();
extern "C" int nuvio_vlc_track_default(libvlc_media_t *, int);
extern "C" void vlc_ps5_vdec_prepare();
static std::atomic<bool> vlc_hw_failed{false};
extern "C" int vlc_ps5_vdec_available() {
    return HwDecoder::module_loaded();
}
extern "C" void vlc_ps5_vdec_failed() {
    vlc_hw_failed = true;
}
extern "C" void vlc_ps5_vdec_active(int) {}
#endif
struct VlcBackend::Impl {
    libvlc_instance_t *instance = nullptr;
    libvlc_media_player_t *player = nullptr;
    Options options;
    long stream_serial = 0;
    std::atomic<bool> stop{false}, failed{false}, user_paused{false}, audio_paused{true};
    std::atomic<int> epoch{1}, shown_epoch{-1}, shown{0}, received{0}, dropped{0};
    std::atomic<float> gain{1};
    std::atomic<int> buffer_percent{0};
    std::atomic<double> seek_target{-1};
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::string failure;
    VideoFrame current, ready;
    VideoPicture picture;
    bool ready_valid = false, opened_picture = false;
    nuvio_vlc_colour colour{0, 1, 0, 0, 1000, 1, 1, 0, 0, 0, 0};
    unsigned width = 0, height = 0, pitch = 0, uvpitch = 0;
    bool ten = false, interleaved = true;
    uint64_t ticket = 0;
    struct Slot {
        std::vector<uint8_t> y, uv, v;
        VideoFrame frame;
        uint64_t ticket = 0;
        int state = 0, epoch = 0;
    };
    std::array<Slot, 4> slots; // three normal buffers and a bounded stop/failure scratch buffer
    struct Audio {
        std::vector<uint8_t> bytes;
        int64_t pts;
        int epoch;
    };
    std::deque<Audio> audio;
    size_t audio_bytes = 0;
    int64_t paused_at = 0;
    PcmOut pcm;
    std::thread audio_worker;
    int selected_sub = -2;
    int restore_audio = -1, restore_sub = -2;
    int64_t sub_delay = INT64_MIN;
    double logged_at = 0, video_wait_since = 0;
    void fail(const char *why) {
        std::lock_guard<std::mutex> lock(mutex);
        failure = why;
        failed = true;
        cv.notify_all();
    }
    static void event(const libvlc_event_t *e, void *opaque) {
        auto &s = *static_cast<Impl *>(opaque);
        if (e->type == libvlc_MediaPlayerBuffering)
            s.buffer_percent = std::clamp(int(e->u.media_player_buffering.new_cache), 0, 100);
        if (e->type == libvlc_MediaPlayerEncounteredError)
            s.fail("VLC could not read or decode this stream. Try Native playback.");
    }
    static void colours(void *opaque, const nuvio_vlc_colour *c) {
        auto &s = *static_cast<Impl *>(opaque);
        std::lock_guard<std::mutex> lock(s.mutex);
        s.colour = *c;
        if (c->transfer == 2) {
            s.failed = true;
            s.failure = "VLC HLG output is not supported yet. Try Native playback.";
        }
    }
    static uint8_t *aligned(std::vector<uint8_t> &bytes) {
        return reinterpret_cast<uint8_t *>((reinterpret_cast<uintptr_t>(bytes.data()) + 63) & ~uintptr_t(63));
    }
    static unsigned setup(void **opaque, char *chroma, unsigned *w, unsigned *h, unsigned *pitches,
                          unsigned *lines) {
        auto &s = *static_cast<Impl *>(*opaque);
        // Preserve padded decoder dimensions for zero-offset pictures. VLC's
        // vmem backport keeps the visible rectangle and avoids CPU resizing.
        // Nonzero crop offsets still use VLC's existing conversion path.
        {
            std::lock_guard<std::mutex> guard(s.mutex);
            if (s.colour.x_offset || s.colour.y_offset) {
                if (s.colour.visible_width && s.colour.visible_width <= *w) *w = s.colour.visible_width;
                if (s.colour.visible_height && s.colour.visible_height <= *h) *h = s.colour.visible_height;
            }
        }
        if (!video_dimensions_valid(int(*w), int(*h))) {
            s.fail("VLC reported unsupported video dimensions");
            return 0;
        }
        const bool ten = !std::memcmp(chroma, "P010", 4) || !std::memcmp(chroma, "I0A", 3) ||
                         !std::memcmp(chroma, "I2A", 3) || !std::memcmp(chroma, "I4A", 3);
        std::lock_guard<std::mutex> lock(s.mutex);
        s.ten = ten;
        // Keep planar decoder output planar. Requesting NV12/P010 for every
        // source adds an avoidable VLC chroma conversion on the console.
        s.interleaved = !std::memcmp(chroma, "NV12", 4) || !std::memcmp(chroma, "P010", 4);
        s.width = *w;
        s.height = *h;
        s.pitch = (*w * (ten ? 2 : 1) + 63) & ~63u;
        s.uvpitch = (((*w + 1) / 2) * (ten ? 2 : 1) * (s.interleaved ? 2 : 1) + 63) & ~63u;
        std::memcpy(chroma, s.interleaved ? (ten ? "P010" : "NV12") : (ten ? "I0AL" : "I420"), 4);
        pitches[0] = s.pitch;
        pitches[1] = s.uvpitch;
        pitches[2] = s.interleaved ? 0 : s.uvpitch;
        lines[0] = *h;
        lines[1] = (*h + 1) / 2;
        lines[2] = s.interleaved ? 0 : (*h + 1) / 2;
        try {
            for (auto &slot : s.slots) {
                slot.y.resize(size_t(s.pitch) * lines[0] + 63);
                slot.uv.resize(size_t(s.uvpitch) * lines[1] + 63);
                slot.v.resize(s.interleaved ? 0 : size_t(s.uvpitch) * lines[2] + 63);
                slot.state = 0;
                slot.ticket = ++s.ticket;
            }
        } catch (...) {
            s.failure = "VLC could not allocate video buffers";
            s.failed = true;
            return 0;
        }
        s.ready_valid = false;
        dlog("player: VLC picture layout %.4s coded=%ux%u visible=%ux%u pitch=%u/%u/%u bits=%d shift=%d",
             chroma, *w, *h, s.colour.visible_width, s.colour.visible_height,
             pitches[0], pitches[1], pitches[2], ten ? 10 : 8, ten && s.interleaved ? 6 : 0);
        return 3;
    }
    static void cleanup(void *) {}
    static void *lock(void *opaque, void **planes) {
        auto &s = *static_cast<Impl *>(opaque);
        std::unique_lock<std::mutex> guard(s.mutex);
        // READY slots can be recycled if VLC skipped their display callback.
        // Each ticket includes a sequence, so a late callback cannot show reuse.
        int index = -1;
        for (int n = 0; n < 20 && index < 0; ++n) {
            for (size_t i = 0; i < 3; ++i)
                if (s.slots[i].state != 1) {
                    index = int(i);
                    break;
                }
            if (index < 0)
                s.cv.wait_for(guard, std::chrono::milliseconds(5));
        }
        if (index < 0) {
            s.failed = true;
            s.failure = "VLC video buffers stopped recycling";
            index = 3;
        }
        auto &slot = s.slots[size_t(index)];
        slot.state = 1;
        slot.epoch = s.epoch;
        slot.ticket = (++s.ticket << 2) | uint64_t(index);
        planes[0] = aligned(slot.y);
        planes[1] = aligned(slot.uv);
        planes[2] = s.interleaved ? nullptr : aligned(slot.v);
        return reinterpret_cast<void *>(uintptr_t(slot.ticket));
    }
    static void unlock(void *opaque, void *id, void *const *) {
        auto &s = *static_cast<Impl *>(opaque);
        const auto key = uint64_t(reinterpret_cast<uintptr_t>(id));
        std::lock_guard<std::mutex> guard(s.mutex);
        auto &slot = s.slots[size_t(key & 3)];
        if (slot.ticket != key || slot.state != 1)
            return;
        if (s.stop || slot.epoch != s.epoch) {
            slot.state = 0;
            s.cv.notify_all();
            return;
        }
        YuvPicture input;
        input.y = aligned(slot.y);
        input.u = aligned(slot.uv);
        input.v = s.interleaved ? nullptr : aligned(slot.v);
        input.width = int(s.colour.visible_width && s.colour.visible_width <= s.width ? s.colour.visible_width : s.width);
        input.height = int(s.colour.visible_height && s.colour.visible_height <= s.height ? s.colour.visible_height : s.height);
        input.y_stride = int(s.pitch);
        input.c_stride = int(s.uvpitch);
        input.interleaved = s.interleaved;
        input.wide = s.ten;
        input.shift = s.ten && s.interleaved ? 6 : 0;
        bool packed = false;
        try {
            packed = pack_video_yuv(input, slot.frame);
        } catch (...) {
        }
        if (!packed) {
            s.failed = true;
            s.failure = "VLC picture conversion failed";
            slot.state = 0;
            return;
        }
        slot.frame.colors = s.colour.transfer == 1 ? YuvColors::Hdr10
                            : s.colour.matrix == 0 ? YuvColors::Bt601
                                                   : YuvColors::Bt709;
        slot.frame.full_range = s.colour.full;
        slot.frame.hdr10_compatible = s.colour.compatible;
        slot.frame.hdr_peak_nits = s.colour.peak;
        slot.frame.sar = s.colour.sar_den ? double(s.colour.sar_num) / s.colour.sar_den : 1;
        slot.state = 2;
        s.cv.notify_all();
    }
    static void display(void *opaque, void *id) {
        auto &s = *static_cast<Impl *>(opaque);
        const auto key = uint64_t(reinterpret_cast<uintptr_t>(id));
        std::lock_guard<std::mutex> guard(s.mutex);
        auto &slot = s.slots[size_t(key & 3)];
        if (slot.ticket != key || slot.state != 2 || slot.epoch != s.epoch || s.stop)
            return;
        if (s.ready_valid)
            ++s.dropped;
        std::swap(s.ready, slot.frame);
        s.ready_valid = true;
        s.opened_picture = true;
        slot.state = 0;
        ++s.received;
        s.cv.notify_all();
    }
    static int audio_setup(void **opaque, char *format, unsigned *rate, unsigned *channels) {
        auto &s = *static_cast<Impl *>(*opaque);
        if (s.stop)
            return -1;
        std::lock_guard<std::mutex> guard(s.mutex);
        if (!s.pcm.open(s.options.surround && *channels > 2)) {
            // This callback already owns mutex; fail() would lock it again.
            s.failure = "VLC could not open PCM audio";
            s.failed = true;
            s.cv.notify_all();
            return -1;
        }
        std::memcpy(format, s.pcm.floating_point() ? "FL32" : "S16N", 4);
        *rate = 48000;
        *channels = unsigned(s.pcm.channels());
        s.audio_paused = s.user_paused.load();
        s.pcm.pause(true);
        return 0;
    }
    static void audio_cleanup(void *opaque) {
        auto &s = *static_cast<Impl *>(opaque);
        std::lock_guard<std::mutex> guard(s.mutex);
        s.audio_paused = true;
        s.audio.clear();
        s.audio_bytes = 0;
        s.pcm.close();
        s.cv.notify_all();
    }
    static void audio_play(void *opaque, const void *data, unsigned frames, int64_t pts) {
        auto &s = *static_cast<Impl *>(opaque);
        if (s.stop)
            return;
        Audio b;
        b.pts = pts;
        b.epoch = s.epoch;
        const size_t channels = size_t(s.pcm.channels()), bytes_per_sample = s.pcm.floating_point() ? 4 : 2;
        if (frames > 48000 * 2) {
            s.fail("VLC audio block exceeded its limit");
            return;
        }
        try {
            b.bytes.resize(size_t(frames) * channels * bytes_per_sample);
        } catch (...) {
            s.fail("VLC could not allocate audio");
            return;
        }
        const int order[8] = {0, 1, 6, 7, 2, 3, 4, 5};
        const float volume = s.gain;
        for (size_t f = 0; f < frames; ++f)
            for (size_t c = 0; c < channels; ++c) {
                const size_t from = f * channels + (channels == 8 ? size_t(order[c]) : c),
                             to = f * channels + c;
                if (bytes_per_sample == 4)
                    reinterpret_cast<float *>(b.bytes.data())[to] =
                        static_cast<const float *>(data)[from] * volume;
                else
                    reinterpret_cast<int16_t *>(b.bytes.data())[to] = int16_t(std::clamp(
                        float(static_cast<const int16_t *>(data)[from]) * volume, -32768.f, 32767.f));
            }
        std::unique_lock<std::mutex> lock(s.mutex);
        if (s.stop || b.epoch != s.epoch)
            return;
        // Bound the callback queue by time, including tiny TrueHD blocks.
        const size_t limit = 48000 * channels * bytes_per_sample * 4;
        if (b.bytes.size() > limit) {
            s.failure = "VLC audio block exceeded its limit";
            s.failed = true;
            return;
        }
        while (!s.stop && !s.failed && b.epoch == s.epoch && s.audio_bytes > limit - b.bytes.size())
            s.cv.wait_for(lock, std::chrono::milliseconds(5));
        if (s.stop || s.failed || b.epoch != s.epoch)
            return;
        s.audio_bytes += b.bytes.size();
        s.audio.push_back(std::move(b));
        s.cv.notify_all();
    }
    static void audio_pause(void *opaque, int64_t at) {
        auto &s = *static_cast<Impl *>(opaque);
        std::lock_guard<std::mutex> guard(s.mutex);
        s.paused_at = at;
        s.audio_paused = true;
        s.pcm.pause(true);
        s.cv.notify_all();
    }
    static void audio_resume(void *opaque, int64_t at) {
        auto &s = *static_cast<Impl *>(opaque);
        std::lock_guard<std::mutex> guard(s.mutex);
        // amem timestamps are wall-clock deadlines. A paused queue resumes at
        // new deadlines rather than playing everything as already-late audio.
        if (s.paused_at && at > s.paused_at)
            for (auto &b : s.audio)
                b.pts += at - s.paused_at;
        s.paused_at = 0;
        s.audio_paused = false;
        s.cv.notify_all();
    }
    static void audio_flush(void *opaque, int64_t) {
        auto &s = *static_cast<Impl *>(opaque);
        std::lock_guard<std::mutex> lock(s.mutex);
        s.audio.clear();
        s.audio_bytes = 0;
        s.pcm.clear();
        s.cv.notify_all();
    }
    static void audio_drain(void *opaque) {
        auto &s = *static_cast<Impl *>(opaque);
        std::unique_lock<std::mutex> guard(s.mutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!s.stop && !s.failed && !s.user_paused && (!s.audio.empty() || s.pcm.queued_seconds() > .02)) {
            if (s.cv.wait_until(guard, std::min(deadline, std::chrono::steady_clock::now() +
                                                              std::chrono::milliseconds(5))) ==
                    std::cv_status::timeout &&
                std::chrono::steady_clock::now() >= deadline)
                break;
        }
    }
    static void volume(void *opaque, float v, bool mute) {
        static_cast<Impl *>(opaque)->gain = mute ? 0 : std::clamp(v, 0.f, 2.f);
    }
    void sound() {
        std::unique_lock<std::mutex> guard(mutex);
        while (!stop) {
            if (pcm.failed()) {
                failed = true;
                failure = "VLC PCM output stopped. Try Native playback.";
                break;
            }
            if (audio_paused || user_paused) {
                pcm.pause(true);
                cv.wait_for(guard, std::chrono::milliseconds(5));
                continue;
            }
            if (audio.empty()) {
                cv.wait_for(guard, std::chrono::milliseconds(5));
                continue;
            }
            auto &b = audio.front();
            if (b.epoch != epoch) {
                audio_bytes -= b.bytes.size();
                audio.pop_front();
                continue;
            }
            const int64_t wait = b.pts - libvlc_clock() - 10667;
            if (wait > 1000) {
                cv.wait_for(guard, std::chrono::microseconds(std::min<int64_t>(wait, 5000)));
                continue;
            }
            if (pcm.queued_seconds() > .1) {
                cv.wait_for(guard, std::chrono::milliseconds(2));
                continue;
            }
            pcm.pause(false);
            if (!pcm.queue(b.bytes.data(), b.bytes.size())) {
                cv.wait_for(guard, std::chrono::milliseconds(2));
                continue;
            }
            audio_bytes -= b.bytes.size();
            audio.pop_front();
            cv.notify_all();
        }
    }
};
VlcBackend::VlcBackend() : d_(std::make_unique<Impl>()) {}
VlcBackend::~VlcBackend() {
    close();
}
bool VlcBackend::available() {
    return true;
}
void VlcBackend::prepare() {
#ifdef NUVIO_VLC_STATIC
    if (HwDecoder::module_loaded())
        vlc_ps5_vdec_prepare();
#endif
}
bool VlcBackend::open(const Options &o) {
    close();
    d_ = std::make_unique<Impl>();
    auto &s = *d_;
    s.options = o;
    s.stream_serial = nuvio_vlc_stream_begin();
    if(o.start>0)s.seek_target=o.start;
    s.user_paused = o.paused;
    s.restore_audio = o.audio;
    s.restore_sub = o.subtitle;
#ifdef NUVIO_VLC_STATIC
    nuvio_vlc_static_anchor();
    prepare(); // UI thread, before libVLC starts any decoding workers
    vlc_hw_failed = false;
#endif
    const char *args[] = {"--ignore-config",       "--no-media-library", "--no-osd",
                          "--no-video-title-show", "--no-stats",         "--no-snapshot-preview",
                          "--aout=amem",           "--vout=vmem",        "--quiet"};
    s.instance = libvlc_new(int(std::size(args)), args);
    if (!s.instance) {
        s.fail("VLC could not initialize");
        return false;
    }
    auto *media = libvlc_media_new_location(s.instance, o.url.c_str());
    if (!media) {
        s.fail("VLC could not open this stream");
        return false;
    }
    const auto option = [&](const std::string &text) {
        libvlc_media_add_option_flag(media, text.c_str(), libvlc_media_option_trusted);
    };
#ifdef NUVIO_VLC_STATIC
    option(":freetype-font=/app0/fonts/NotoSans-Regular.ttf");
    option(":freetype-monofont=/app0/fonts/NotoSans-Regular.ttf");
#endif
    if (o.software) {
        // VLC boolean options use the no- prefix; '=0' can still enable them.
        // Restrict the codec list as well so fallback cannot reselect ps5vdec.
        option(":no-ps5vdec");
        option(":codec=avcodec");
        option(":avcodec-hw=none");
    }
    option(":adaptive-use-access=1");
    option(":nuvio-stream-serial=" + std::to_string(s.stream_serial));
    option(":sub-track=-1");
    option(":network-caching=2000");
    option(":file-caching=500");
    if (o.start > 0)
        option(":start-time=" + std::to_string(o.start));
    if (o.paused)
        option(":start-paused");
    if (!o.audio_languages.empty()) {
        std::string langs;
        for (const auto &v : o.audio_languages) {
            if (!langs.empty())
                langs += ',';
            langs += v;
        }
        option(":audio-language=" + langs);
    }
    if (!o.headers.empty())
        option(":nuvio-http-headers=" + nlohmann::json(o.headers).dump());
    s.player = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    if (!s.player) {
        s.fail("VLC could not create the player");
        return false;
    }
    libvlc_event_attach(libvlc_media_player_event_manager(s.player), libvlc_MediaPlayerEncounteredError,
                        Impl::event, &s);
    libvlc_event_attach(libvlc_media_player_event_manager(s.player), libvlc_MediaPlayerBuffering, Impl::event,
                        &s);
    libvlc_video_set_callbacks(s.player, Impl::lock, Impl::unlock, Impl::display, &s);
    libvlc_video_set_format_callbacks(s.player, Impl::setup, Impl::cleanup);
#ifdef NUVIO_VLC_STATIC
    nuvio_vlc_colour_install(s.player, Impl::colours);
#endif
    libvlc_audio_set_callbacks(s.player, Impl::audio_play, Impl::audio_pause, Impl::audio_resume,
                               Impl::audio_flush, Impl::audio_drain, &s);
    libvlc_audio_set_format_callbacks(s.player, Impl::audio_setup, Impl::audio_cleanup);
    libvlc_audio_set_volume_callback(s.player, Impl::volume);
    try {
        s.audio_worker = std::thread([&s] { s.sound(); });
    } catch (...) {
        s.fail("VLC could not start its audio worker");
        return false;
    }
    if (libvlc_media_player_play(s.player) < 0) {
        s.fail("VLC could not start this stream");
        return false;
    }
    s.audio_paused = false;
    dlog("player: VLC %s; decoded PCM output", libvlc_get_version());
    return true;
}
void VlcBackend::close() {
    if (!d_)
        return;
    auto &s = *d_;
    s.stop = true;
    nuvio_vlc_stream_end(s.stream_serial);
    s.cv.notify_all();
    if (s.player) {
        libvlc_media_player_stop(s.player);
        libvlc_media_player_release(s.player);
        s.player = nullptr;
    }
    if (s.audio_worker.joinable())
        s.audio_worker.join();
    s.pcm.close();
    if (s.instance) {
        libvlc_release(s.instance);
        s.instance = nullptr;
    }
}
int VlcBackend::state() const {
    const auto &s = *d_;
#ifdef NUVIO_VLC_STATIC
    if (vlc_hw_failed && !s.options.software)
        return 1; // allow the UI's present() call to recover
#endif
    if (s.failed)
        return 4;
    if (!s.player)
        return 0;
    const auto state = libvlc_media_player_get_state(s.player);
    if (state == libvlc_Error)
        return 4;
    if (state == libvlc_Ended) {
        std::lock_guard<std::mutex> g(s.mutex);
        return s.opened_picture || s.pcm.is_open() ? 3 : 4;
    }
    const bool audio_only = libvlc_video_get_track_count(s.player) == 0 && libvlc_audio_get_track_count(s.player) > 0;
    std::lock_guard<std::mutex> g(s.mutex);
    return s.opened_picture || (audio_only && s.pcm.is_open() && (state == libvlc_Playing || state == libvlc_Paused)) ? 2
                                                                                                        : 1;
}
std::string VlcBackend::error() const {
    std::lock_guard<std::mutex> g(d_->mutex);
    return d_->failure.empty() ? "VLC could not play this stream. Try Native playback." : d_->failure;
}
bool VlcBackend::paused() const {
    return d_->user_paused;
}
void VlcBackend::set_paused(bool p) {
    d_->user_paused = p;
    if (d_->player)
        libvlc_media_player_set_pause(d_->player, p);
    d_->cv.notify_all();
}
bool VlcBackend::buffering(int *percent) const {
    const int s = state();
    const bool b = d_->seek_target >= 0 || s == 1 ||
                   (d_->player && libvlc_media_player_get_state(d_->player) == libvlc_Buffering);
    if (percent)
        *percent = b ? d_->buffer_percent.load() : 100;
    return b;
}
double VlcBackend::position() const {
    const double pending = d_->seek_target;
    if (pending >= 0)
        return pending;
    return d_->player ? std::max(0.0, double(libvlc_media_player_get_time(d_->player)) / 1000) : 0;
}
double VlcBackend::duration() const {
    return d_->player ? std::max(0.0, double(libvlc_media_player_get_length(d_->player)) / 1000) : 0;
}
void VlcBackend::seek(double t) {
    if (!d_->player || !std::isfinite(t))
        return;
    const double length = duration();
    if (length > 0)
        t = std::min(t, std::max(0.0, length - 2));
    t = std::max(0.0, t);
    if (libvlc_media_player_get_state(d_->player) == libvlc_Ended) {
        auto options = d_->options;
        options.start = t;
        options.paused = paused();
        open(options);
        d_->seek_target = t;
        return;
    }
    if (!libvlc_media_player_is_seekable(d_->player))
        return;
    d_->seek_target = std::max(0.0, t);
    {
        std::lock_guard<std::mutex> g(d_->mutex);
        ++d_->epoch;
        d_->ready_valid = false;
        d_->audio.clear();
        d_->audio_bytes = 0;
        d_->pcm.clear();
    }
    libvlc_media_player_set_time(d_->player, int64_t(std::max(0.0, t) * 1000));
    d_->cv.notify_all();
}
const VideoPicture *VlcBackend::present() {
#ifdef NUVIO_VLC_STATIC
    if (vlc_hw_failed.exchange(false) && !d_->options.software) {
        auto o = d_->options;
        o.start = d_->seek_target >= 0 ? d_->seek_target.load() : position();
        o.paused = paused();
        o.software = true;
        dlog("player: VLC hardware decoder failed; reopening with software decoding");
        open(o);
        return nullptr;
    }
#endif
    if (d_->player && libvlc_media_player_get_state(d_->player) == libvlc_Playing) {
        bool no_picture;
        { std::lock_guard<std::mutex> g(d_->mutex); no_picture = !d_->opened_picture; }
        if (no_picture && !d_->user_paused && d_->buffer_percent >= 100 && libvlc_video_get_track_count(d_->player) > 0) {
            const double now = now_seconds();
            if (d_->video_wait_since <= 0) d_->video_wait_since = now;
            if (now - d_->video_wait_since >= 10) {
                if (!d_->options.software) {
                    auto options = d_->options;
                    options.start = d_->seek_target >= 0 ? d_->seek_target.load() : position();
                    options.paused = paused(); options.software = true;
                    dlog("player: VLC video startup timed out; retrying same source in software");
                    open(options);
                } else d_->fail("VLC audio started without a video picture. Try Native playback.");
                return nullptr;
            }
        } else d_->video_wait_since = 0;
        if (d_->restore_audio >= 0 && libvlc_audio_set_track(d_->player, d_->restore_audio) == 0)
            d_->restore_audio = -1;
        if (d_->restore_sub >= -1 && libvlc_video_set_spu(d_->player, d_->restore_sub) == 0) {
            d_->selected_sub = d_->restore_sub;
            d_->restore_sub = -2;
            libvlc_video_set_spu_delay(d_->player, int64_t(d_->options.subtitle_delay * 1000000));
        }
    }
    // VLC may hold its input lock while invoking our audio/video callbacks.
    // Read its clock before taking the callback mutex to avoid lock inversion
    // while a seek is waiting for its first picture.
    const double playback_time = d_->seek_target >= 0 && d_->player
        ? double(libvlc_media_player_get_time(d_->player)) / 1000 : 0;
    std::lock_guard<std::mutex> g(d_->mutex);
    if (!d_->ready_valid)
        return nullptr;
    const double target = d_->seek_target;
    if (target >= 0 && std::abs(playback_time - target) > 2)
        return nullptr;
    std::swap(d_->current, d_->ready);
    d_->seek_target = -1;
    d_->ready_valid = false;
    d_->picture = d_->current.picture();
    d_->shown_epoch = d_->epoch.load();
    ++d_->shown;
    return &d_->picture;
}
int VlcBackend::serial() const {
    return d_->epoch;
}
int VlcBackend::picture_serial() const {
    return d_->shown_epoch;
}
std::vector<VlcBackend::Track> VlcBackend::tracks(bool subtitles) const {
    std::vector<Track> out;
    if (!d_->player)
        return out;
    auto *m = libvlc_media_player_get_media(d_->player);
    if (!m)
        return out;
    libvlc_media_track_t **list = nullptr;
    const auto count = libvlc_media_tracks_get(m, &list);
    const int chosen = subtitles ? libvlc_video_get_spu(d_->player) : libvlc_audio_get_track(d_->player);
    for (unsigned i = 0; i < count; ++i) {
        const auto *t = list[i];
        if (t->i_type != (subtitles ? libvlc_track_text : libvlc_track_audio))
            continue;
        Track v;
        v.stream = t->i_id;
        v.lang = t->psz_language ? t->psz_language : "";
        v.label = t->psz_description ? t->psz_description : v.lang;
        if (v.label.empty())
            v.label = subtitles ? "Embedded subtitles" : "Audio track";
        v.is_default = t->i_id == chosen;
#ifdef NUVIO_VLC_STATIC
        v.is_default = nuvio_vlc_track_default(m, t->i_id);
#endif
        out.push_back(std::move(v));
    }
    libvlc_media_tracks_release(list, count);
    libvlc_media_release(m);
    return out;
}
int VlcBackend::audio_stream() const {
    return d_->player ? libvlc_audio_get_track(d_->player) : -1;
}
void VlcBackend::select_audio(int s) {
    d_->options.audio = s;
    if (!d_->player || s == audio_stream())
        return;
    const double at = position();
    if (libvlc_audio_set_track(d_->player, s) == 0 && libvlc_media_player_is_seekable(d_->player))
        seek(at);
}
void VlcBackend::select_subtitle(int s, double delay) {
    d_->options.subtitle = s;
    d_->options.subtitle_delay = std::isfinite(delay) ? delay : 0;
    if (!d_->player)
        return;
    if (s != d_->selected_sub) {
        const double at = position();
        if (libvlc_video_set_spu(d_->player, s) == 0) {
            d_->selected_sub = s;
            if (s >= 0 && libvlc_media_player_is_seekable(d_->player))
                seek(at);
        }
    }
    const int64_t us = std::isfinite(delay) ? int64_t(delay * 1000000) : 0;
    if (us != d_->sub_delay) {
        libvlc_video_set_spu_delay(d_->player, us);
        d_->sub_delay = us;
    }
}
std::string VlcBackend::audio_output() const {
    std::lock_guard<std::mutex> guard(d_->mutex);
    return d_->pcm.is_open() ? d_->pcm.description() : "unavailable";
}
std::string VlcBackend::codec_summary() const {
    std::string result = "VLC";
    if (d_->player) {
        auto *media = libvlc_media_player_get_media(d_->player);
        if (media) {
            libvlc_media_track_t **tracks = nullptr;
            const unsigned n = libvlc_media_tracks_get(media, &tracks);
            for (unsigned i = 0; i < n; ++i)
                if (tracks[i]->i_type == libvlc_track_video) {
                    const auto &v = *tracks[i]->video;
                    result += " " + std::to_string(v.i_width) + "x" + std::to_string(v.i_height);
                    if (v.i_frame_rate_den && v.i_frame_rate_num)
                        result +=
                            " · " +
                            std::to_string(double(v.i_frame_rate_num) / v.i_frame_rate_den).substr(0, 4) +
                            " fps";
                    break;
                }
            libvlc_media_tracks_release(tracks, n);
            libvlc_media_release(media);
        }
    }
    return result + " · " + audio_output();
}
std::string VlcBackend::stats() const {
    return codec_summary() + " · shown " + std::to_string(d_->shown.load()) + " · UI skipped " +
           std::to_string(d_->dropped.load());
}
void VlcBackend::log_stats() {
    const double now = now_seconds();
    if (now - d_->logged_at >= 30) {
        d_->logged_at = now;
        dlog("player: %s", stats().c_str());
    }
}
#else
struct VlcBackend::Impl {};
VlcBackend::VlcBackend() : d_(std::make_unique<Impl>()) {}
VlcBackend::~VlcBackend() = default;
void VlcBackend::prepare() {}
bool VlcBackend::available() {
    return false;
}
bool VlcBackend::open(const Options &) {
    return false;
}
void VlcBackend::close() {}
int VlcBackend::state() const {
    return 4;
}
std::string VlcBackend::error() const {
    return "VLC is not included in this build";
}
bool VlcBackend::paused() const {
    return false;
}
void VlcBackend::set_paused(bool) {}
bool VlcBackend::buffering(int *p) const {
    if (p)
        *p = 0;
    return false;
}
double VlcBackend::position() const {
    return 0;
}
double VlcBackend::duration() const {
    return 0;
}
void VlcBackend::seek(double) {}
const VideoPicture *VlcBackend::present() {
    return nullptr;
}
int VlcBackend::serial() const {
    return 0;
}
int VlcBackend::picture_serial() const {
    return -1;
}
std::vector<VlcBackend::Track> VlcBackend::tracks(bool) const {
    return {};
}
int VlcBackend::audio_stream() const {
    return -1;
}
void VlcBackend::select_audio(int) {}
void VlcBackend::select_subtitle(int, double) {}
std::string VlcBackend::audio_output() const {
    return "unavailable";
}
std::string VlcBackend::codec_summary() const {
    return "VLC unavailable";
}
std::string VlcBackend::stats() const {
    return codec_summary();
}
void VlcBackend::log_stats() {}
#endif
