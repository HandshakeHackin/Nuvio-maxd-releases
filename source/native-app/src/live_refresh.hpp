// Profile-local channel-list refresh. Uses monotonic time; no wake/background service.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "util.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace sx::live_refresh {
inline constexpr const char* preference_key = "playlist_refresh_hours";
inline constexpr int default_hours = 6;
struct Choice { int hours; const char* label; };
inline constexpr std::array<Choice,6> choices{{
    {0,"Off"}, {1,"Every hour"}, {3,"Every 3 hours"},
    {6,"Every 6 hours"}, {12,"Every 12 hours"}, {24,"Every 24 hours"}
}};
inline bool valid(const json& state) {
    if (!state.contains(preference_key)) return true;
    const auto& value = state.at(preference_key);
    if (!value.is_number_integer()) return false;
    return std::any_of(choices.begin(), choices.end(), [&](const auto& choice) { return value == choice.hours; });
}
inline int hours(const json& state) {
    if (valid(state) && state.contains(preference_key)) return state.at(preference_key).get<int>();
    return default_hours;
}
inline std::size_t index(int hours) {
    for (std::size_t n=0; n<choices.size(); ++n) if (choices[n].hours==hours) return n;
    return 3;
}
enum class Outcome { success, failure, deferred };
class Timer {
    int hours_ = default_hours;
    unsigned failures_ = 0;
    bool armed_ = false;
    double next_ = 0;
    double interval() const { return hours_ ? double(hours_)*3600 : std::numeric_limits<double>::infinity(); }
public:
    void reset() { *this = Timer{}; }
    void configure(int hours, double now) {
        hours_ = choices[index(hours)].hours;
        failures_ = 0;
        if (armed_) next_ = now + interval();
    }
    void finished(double now, Outcome outcome) {
        armed_ = true;
        if (outcome == Outcome::success) {
            failures_ = 0;
            next_ = now + interval();
        } else if (outcome == Outcome::deferred) {
            next_ = now + 30;
        } else {
            failures_ = std::min(failures_+1, 5u);
            const double backoff = std::min(300.0 * double(1u << (failures_-1)), 3600.0);
            next_ = now + std::min(interval(), backoff);
        }
    }
    bool due(double now) const { return hours_>0 && armed_ && std::isfinite(now) && now>=next_; }
    double next() const { return next_; }
    unsigned failures() const { return failures_; }
};
} // namespace sx::live_refresh
