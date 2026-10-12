// Bound submissions to the refresh rate accepted by the display. Swap may
// return before scanout completes; do not render duplicate frames unbounded.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sx {
class FramePacer {
public:
    void configure(int refresh_hz, std::int64_t now_us) {
        const int hz = refresh_hz == 120 ? 120 : 60;
        if (hz == hz_ && next_us_ != 0) return;
        hz_ = hz;
        next_us_ = double(now_us);
    }
    std::uint32_t delay_us(std::int64_t now_us) const {
        return std::uint32_t(std::clamp(std::ceil(next_us_ - double(now_us)), 0.0, period_us()));
    }
    void started(std::int64_t now_us) {
        // After a hitch, start from now; never run a burst to catch up. Small
        // timer rounding keeps the original phase instead of accumulating.
        if (double(now_us) - next_us_ > period_us()) next_us_ = double(now_us);
        next_us_ += period_us();
    }
    int refresh_hz() const { return hz_; }
private:
    double period_us() const { return 1000000.0 / hz_; }
    double next_us_ = 0;
    int hz_ = 60;
};
} // namespace sx
