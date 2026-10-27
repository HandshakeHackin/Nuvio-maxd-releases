// Real host libVLC with fake console audio; no fake decoder or VLC API.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util.h"
#include "vlc_backend.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
static std::atomic<unsigned> audio_writes{0};
static std::atomic<int> audio_peak{0};
static std::atomic<bool> fail_audio_open{false};
extern "C" int sceAudioOutInit() {
    return 0;
}
extern "C" int sceAudioOutOpen(int, int, int, unsigned, unsigned, unsigned) {
    return fail_audio_open ? -1 : 42;
}
extern "C" int sceAudioOutClose(int) {
    return 0;
}
extern "C" int sceAudioOutOutput(int, const void *data) {
    ++audio_writes;
    if (data) {
        const auto *p = static_cast<const int16_t *>(data);
        for (int i = 0; i < 512; ++i)
            if (std::abs(int(p[i])) > audio_peak)
                audio_peak = std::abs(int(p[i]));
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(5333333));
    return 0;
}
extern "C" int sceAudioOutExOpen(int, int) {
    return -1;
}
extern "C" int sceAudioOutExClose(int) {
    return 0;
}
extern "C" int sceAudioOutExConfigureOutput(int, unsigned, int, int, uint64_t) {
    return -1;
}
extern "C" int sceAudioOutSysGetHdmiMonitorInfo(int, void *, unsigned) {
    return -1;
}
void require(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
template <class F> void wait(VlcBackend &b, F f, double seconds = 12) {
    const double until = now_seconds() + seconds;
    while (now_seconds() < until) {
        b.present();
        if (b.state() == 4)
            throw std::runtime_error(b.error());
        if (f())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    throw std::runtime_error("VLC timed out: position=" + std::to_string(b.position()) + " state=" + std::to_string(b.state()) + " serial=" + std::to_string(b.serial()) + " picture=" + std::to_string(b.picture_serial()) + " " + b.stats());
}
int main(int argc, char **argv) try {
    require(argc >= 2, "fixture missing");
    {
        fail_audio_open = true;
        VlcBackend failed;
        VlcBackend::Options o;
        o.url = std::string("file://") + argv[1];
        require(failed.open(o), "audio failure fixture did not open");
        const double until = now_seconds() + 3;
        while (now_seconds() < until && failed.state() != 4)
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        require(failed.state() == 4, "audio open failure did not report an error");
        failed.close();
        fail_audio_open = false;
        std::cerr << "VLC audio open failure reports and closes without deadlock\n";
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]).find("stall-") != std::string::npos) {
            VlcBackend blocked;
            VlcBackend::Options pending;
            pending.url = argv[i];
            pending.headers = {"X-Nuvio-VLC-Test: required"};
            require(blocked.open(pending), "blocked HTTP open failed early");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const double start = now_seconds();
            blocked.close();
            require(now_seconds() - start < 1.5, "VLC HTTP close failed to interrupt headers");
            std::cout << "VLC HTTP wait cancellation passed\n";
            continue;
        }
        audio_peak = 0;
        audio_writes = 0;
        VlcBackend b;
        VlcBackend::Options o;
        o.url = std::string(argv[i]).find("://") != std::string::npos ? argv[i]
                                                                      : std::string("file://") + argv[i];
        o.headers = {"X-Nuvio-VLC-Test: required"};
        require(b.available() && b.open(o), "VLC unavailable");
        wait(b, [&] { return b.state() == 2 && b.picture_serial() == b.serial() && b.position() > .3; });
        require(b.position() < 2, "VLC skipped the requested start");
        require(!b.tracks(false).empty(), "VLC audio metadata missing");
        require(audio_writes > 0 && audio_peak > 100, "VLC audio samples did not reach console boundary");
        const int normal_peak=audio_peak;
        audio_peak=0;b.set_audio_gain(200);b.set_audio_delay(.05);
        wait(b,[&]{return audio_peak>normal_peak*1.6;},3);
        b.set_audio_gain(100);b.set_audio_delay(0);
        const auto audio = b.tracks(false);
        if (audio.size() > 1) {
            b.select_audio(audio.back().stream);
            wait(b, [&] { return b.audio_stream() == audio.back().stream && b.position() < 2; });
        }
        require(!b.tracks(true).empty() || o.url.find(".m3u8") != std::string::npos,
                "embedded subtitle metadata missing");
        std::cerr << "opened " << b.position() << " dur " << b.duration() << "\n";
        b.set_paused(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const double frozen = b.position();
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        require(std::abs(frozen - b.position()) < .08, "VLC paused clock moved");
        std::cerr << "paused " << b.position() << "\n";
        const int before = b.picture_serial();
        b.seek(6);
        wait(b, [&] {
            return b.picture_serial() == b.serial() && b.picture_serial() != before && b.position() >= 5.9;
        });
        std::cerr << "seeked paused " << b.position() << "\n";
        require(b.paused(), "VLC seek lost pause");
        b.set_paused(false);
        wait(b, [&] { return b.position() > 6.2; });
        std::cerr << "resumed " << b.position() << "\n";
        b.seek(2);
        // position() reports the pending target before VLC completes its seek.
        // Wait for a picture from this epoch before issuing another seek.
        wait(b, [&] {
            return b.picture_serial() == b.serial() && b.position() >= 1.9 && b.position() < 3;
        });
        std::cerr << "rewound " << b.position() << "\n";
        b.seek(1000000);
        wait(b, [&] { return b.picture_serial() == b.serial() && b.position() >= b.duration() - 2.1; });
        std::cerr << "near end " << b.position() << " dur " << b.duration() << "\n";
        wait(b, [&] { return b.state() == 3; }, 8);
        b.seek(2);
        // Adaptive HLS uses two-second fixture segments and a coarse libVLC clock.
        // Allow one segment of landing tolerance; file replay remains strict.
        std::cerr << "replay requested " << o.url << " pos " << b.position() << " serial " << b.serial() << " picture " << b.picture_serial() << "\n";
        wait(b, [&] {
            return b.state() == 2 && b.picture_serial() == b.serial() && b.position() > 2.1 &&
                   b.position() < (o.url.find(".m3u8") != std::string::npos ? 4.1 : 3.0);
        });
        std::cerr << "replayed " << b.position() << "\n";
        b.close();
        require(b.state() == 0, "VLC close did not release player");
        require(b.open(o), "VLC reopen failed");
        std::cerr << "reopened " << o.url << "\n";
        wait(b, [&] { return b.picture_serial() > 0; });
        std::cerr << "first reopened picture " << b.position() << "\n";
        b.close();
        std::cerr << "reopen closed\n";
        std::cout << "Real VLC: open/video/audio, pause, forward/back seek and reopen passed\n";
    }
    VlcBackend bad;
    VlcBackend::Options o;
    o.url = "file:///nonexistent/nuvio-vlc-test";
    bad.open(o);
    const double until = now_seconds() + 5;
    while (now_seconds() < until && bad.state() != 4)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(bad.state() == 4,
            ("VLC missing input did not report failure; state=" + std::to_string(bad.state())).c_str());
    bad.close();
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
