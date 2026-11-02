// Optional linked-channel upward compression; no lookahead or added latency.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>

class PcmNightMode {
    double envelope_ = 0;
    int previous_ = 0;
public:
    void reset() { envelope_ = 0; previous_ = 0; }
    void process(void* data, size_t frames, int channels, bool floating,
                 int rate, int level) {
        level = std::clamp(level, 0, 3);
        if (level != previous_) { envelope_ = 0; previous_ = level; }
        if (!level || !data || channels < 1 || channels > 8 || rate <= 0) return;
        const double boost[] = {0, 6, 10, 15};
        const double attack = std::exp(-1.0 / (rate * .001));
        const double release = std::exp(-1.0 / (rate * .080));
        constexpr double ceiling = .8912509381337456; // -1 dBFS
        for (size_t f = 0; f < frames; ++f) {
            double peak = 0, values[8];
            for (int c = 0; c < channels; ++c) {
                const size_t i = f * channels + c;
                double v = floating ? static_cast<float*>(data)[i]
                                    : static_cast<int16_t*>(data)[i] / 32768.0;
                values[c] = std::isfinite(v) ? v : 0;
                peak = std::max(peak, std::abs(values[c]));
            }
            const double coefficient = peak > envelope_ ? attack : release;
            envelope_ = coefficient * envelope_ + (1 - coefficient) * peak;
            const double db = 20 * std::log10(std::max(envelope_, 1e-12));
            const double fade = std::clamp((db + 65) / 15, 0.0, 1.0);
            double gain = std::pow(10.0, boost[level] * fade *
                                      std::clamp(-db / 30, 0.0, 1.0) / 20);
            // One gain for every channel: retain balance and bound transients.
            if (peak > 0) gain = std::min(gain, ceiling / peak);
            for (int c = 0; c < channels; ++c) {
                const size_t i = f * channels + c;
                const double v = values[c] * gain;
                if (floating) static_cast<float*>(data)[i] = float(v);
                else static_cast<int16_t*>(data)[i] = int16_t(std::clamp(
                    std::lround(v * 32768), -32768L, 32767L));
            }
        }
    }
};
