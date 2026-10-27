// EVO Player 0.13.0 video engine adapter. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "video_frame.h"
#include "evo_vdec.h"
#include "pp_playback.h"
#include <memory>
#include <string>
struct AVCodecParameters;

class EvoDecoder {
public:
    static std::unique_ptr<EvoDecoder> open(const AVCodecParameters* par, std::string& error);
    ~EvoDecoder();
    int send(const uint8_t* data, int size, int64_t pts);
    int receive(pp_frame& frame);
    bool pack(const pp_frame& frame, VideoFrame& out);
    void flush();
    bool hardware() const;
private:
    EvoDecoder() = default;
    evo_vdec* decoder_ = nullptr;
    int sample_shift_ = -1;
    struct SwsContext* scaler_ = nullptr;
};

// UI-owned EVO presentation core. Nuvio's owned frame queue protects the
// borrowed plane lifetime; the audio output remains the master clock.
class EvoPresenter {
public:
    EvoPresenter();
    ~EvoPresenter();
    void reset();
    int publish(const VideoPicture& picture, double pts, double media_time);
private:
    static int64_t media_clock(void* self);
    pp_playback playback_{};
    int64_t media_us_ = 0;
};
