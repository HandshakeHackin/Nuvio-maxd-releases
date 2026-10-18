#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,2> release_note_items = {
    "New MAX'D family app icon with Nuvio teal, white and pink/purple colours.",
    "Fixed a shutdown crash that could cancel an in-app update before any playback."
};
}
