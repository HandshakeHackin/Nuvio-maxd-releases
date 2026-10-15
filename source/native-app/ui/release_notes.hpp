#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Settings are organised into easy-to-browse categories.",
    "Controller lightbar fades teal, white and pink/purple.",
    "Optional console light sync is in Appearance & Lighting.",
    "Console sync uses approximate colours and restores system lighting.",
    "Reduced Motion keeps lighting steady.",
    "Video timing, PCM audio and stereo downmix improvements are retained."
};
}
