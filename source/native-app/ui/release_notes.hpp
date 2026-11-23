#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Multilingual search, configured-catalog title suggestions and an optional top menu with Search.",
    "Choose a channel-list refresh interval in Settings > Live TV. Playback is left alone.",
    "Personal IPTV and optional free channels share one guide; choose multiple countries for free TV.",
    "Thai subtitle fallback and SIMKL sign-in fixes are included.",
};
}
