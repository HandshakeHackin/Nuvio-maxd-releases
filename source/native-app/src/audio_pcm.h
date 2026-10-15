#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

// Explicit stereo fold-down: preserve dialogue and surround content, include
// some LFE, and normalize the matrix to leave headroom. Stereo stays identity.
inline bool configure_pcm_mix(SwrContext* swr) {
    return av_opt_set_double(swr, "center_mix_level", std::sqrt(0.5), 0) >= 0 &&
           av_opt_set_double(swr, "surround_mix_level", std::sqrt(0.5), 0) >= 0 &&
           av_opt_set_double(swr, "lfe_mix_level", 0.5, 0) >= 0 &&
           av_opt_set_double(swr, "rematrix_maxval", 1.0, 0) >= 0;
}

// The native port can have the playing grain and one queued grain. This is
// an estimate of console buffering, not a measurement of a TV/receiver delay.
inline double pcm_audible_seconds(double submitted, int rate, int grain, bool bitstream) {
    return std::max(0.0, submitted - (bitstream ? 0.0 : 2.0 * grain / rate));
}

struct PcmLevels {
    double peak = 0, squares = 0;
    size_t samples = 0;
    void add(const void* data, size_t count, bool floating) {
        for (size_t i = 0; i < count; ++i) {
            double v = floating ? static_cast<const float*>(data)[i]
                                : static_cast<const int16_t*>(data)[i] / 32768.0;
            if (!std::isfinite(v)) continue;
            peak = std::max(peak, std::abs(v)); squares += v*v; ++samples;
        }
    }
    double peak_db() const { return 20*std::log10(std::max(peak, 1e-6)); }
    double rms_db() const { return 10*std::log10(std::max(samples ? squares/samples : 0.0, 1e-12)); }
};
