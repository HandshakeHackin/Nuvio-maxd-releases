#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "VLC 3.0.24 is available as an optional experimental player.",
    "Choose Native, EVO or VLC in Settings > Video.",
    "VLC supports our playback menus, file subtitles and PCM audio.",
    "Network headers and cancellable downloads are retained for VLC.",
    "Rapid network seeks now keep the correct byte position.",
    "Native remains the default; this build needs PS5 testing."
};
}
