#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Hebrew text and redirected video streams work more reliably.",
    "Profile PINs now use a masked controller-operated numeric keypad.",
    "Fixed incompatible HLS audio choices and unnecessary playlist downloads.",
    "Continue Watching now includes the full list and imports more resume formats.",
    "Improved torrent recovery and preserved progress during tracker outages.",
    "Updated optional VLC decoding, buffering and large-picture handling; PS5 testing needed."
};
}
