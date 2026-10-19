#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Fixed genuine HLS playlists rejected at extensionless provider addresses.",
    "VLC software retry now forces the software decoder instead of retrying PS5 hardware.",
    "Preserve planar VLC pictures and their 8/10-bit sample alignment for correct colours.",
    "Restore VLC picture-memory hooks and use direct decoder workspace when flexible memory is exhausted."
};
}
