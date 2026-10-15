#pragma once
#include "core/tween.hpp"
#include <cmath>
#include <cstdint>

namespace sx {
struct LogoLight { std::uint8_t r, g, b; };
struct LogoAnimation {
    float enter, leave, glow;
    LogoLight light;
};
// One clock and envelope for the on-screen loading glow and DualSense light.
inline LogoAnimation logo_animation(float seconds, float out) {
    const float leave = hui::tween::cubic_out(hui::tween::clamp01(out));
    const float enter = hui::tween::cubic_out(hui::tween::clamp01((seconds - .35f) / .7f));
    const float breathe = hui::tween::reduced_motion ? .5f : .5f + .5f * std::sin(seconds * 2.6f);
    const float glow = enter * (.12f + .10f * breathe);
    // One RGB zone: move through the logo colors in time. Each leg takes
    // six seconds, with zero slope at each color and at the loop boundary.
    const float cycle = hui::tween::reduced_motion ? 0.0f : std::fmod(std::fmax(seconds, 0.0f), 18.0f) / 6.0f;
    const int leg = static_cast<int>(cycle);
    const float blend = .5f - .5f * std::cos((cycle - leg) * 3.14159265358979323846f);
    constexpr float palette[4][3] = {{0,211,255}, {255,255,255}, {146,43,255}, {0,211,255}};
    const float brightness = glow / .22f * (1 - leave) + .35f * leave;
    const auto channel = [brightness](float value) {
        return static_cast<std::uint8_t>(std::lround(value * brightness));
    };
    const auto color = [leg, blend, &palette](int c) {
        return palette[leg][c] + (palette[leg + 1][c] - palette[leg][c]) * blend;
    };
    return {enter, leave, glow, {channel(color(0)), channel(color(1)), channel(color(2))}};
}
}
