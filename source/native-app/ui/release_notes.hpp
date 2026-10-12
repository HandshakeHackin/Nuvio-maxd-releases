#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Fixed a startup halt that left the app on a black screen.",
    "Successful display setup now continues to the loading screen.",
    "Production startup now has a compiler check for conditional control flow.",
    "Experimental HDR and optional HDMI bitstream audio are retained.",
    "PCM remains the default; HDR and receiver output still need console testing.",
    "Reopen these notes from Settings → What's new."
};
}
