#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Updater waits for the backing image to release after app exit; installed files remain protected.",
    "Added an updater handoff test and a persistent diagnostic log for mounted-image failures.",
    "Trakt pairing reports missing app registration clearly and handles expired or declined codes.",
    "Includes the HLS and VLC playback fixes from temporary build 0.5.34.1."
};
}
