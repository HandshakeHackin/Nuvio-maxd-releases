#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.10 experimental: Settings > Controls > Remote buttons can test and assign buttons that reach Nuvio to Search, Options, navigation, playback or seeking. This only changes controls inside Nuvio; PlayStation/TV system buttons may not reach the app. Controller controls stay unchanged.",
    "Optional PCM Night Mode and stereo bass levels, provider guide-clock correction, chapter skip buttons, trailer language preference and Native/EVO subtitle auto-sync are included.",
    "Updated FFmpeg 7.0.3 and XML/image/font libraries, with additional subtitle safety fixes. Native/EVO now omit Dolby Vision enhancement pictures while preserving compatible HDR10 base video.",
    "Auto HDR attempts native HDR10 and converts to SDR if output is rejected. Require native HDR10 and Convert HDR to SDR remain available. Plex, Jellyfin, Emby and previous media artwork/persistence fixes remain included. Real-console testing is still required."
};
}
