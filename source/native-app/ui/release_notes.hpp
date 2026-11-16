#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.28: choose your start page in Settings > App & Updates.",
    "Live TV remembers each source's group. Options on a channel lets you rename or hide it.",
    "Choose backup copies from your current playlist. Restore hidden channels in Sources & filters.",
    "Signed release files and credits are checked offline. Warnings are dismissible; playback remains available.",
};
}
