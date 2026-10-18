#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,3> release_note_items = {
    "Hebrew text now has bundled font fallbacks for menus, titles and subtitles.",
    "Fixed relative HLS segment links after a stream redirects to another address.",
    "Clearer errors for web pages returned as video and hidden torrent results."
};
}
