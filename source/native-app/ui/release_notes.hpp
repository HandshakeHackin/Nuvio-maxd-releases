#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Test fix: changing Native audio tracks preserves playback position and pause state.",
    "Recent torrent sources appear immediately for five minutes while providers refresh; Reload starts fresh.",
    "Safer audio shutdown, video resource cleanup and VLC seek/rebuffer handling.",
    "Retains provider, Trakt pairing and updater fixes from test build 0.5.34.3. Please report console results."
};
}
