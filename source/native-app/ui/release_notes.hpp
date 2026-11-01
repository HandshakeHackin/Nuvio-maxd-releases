#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Test build 1.0.0.4: fixes for external subtitle sizing (#12) and missing Library cover images (#13). Reporter confirmation is pending.",
    "Subtitle playback and preview share Small/Medium/Large sizes. Added Extra large (150%) and Largest (200%) options in Settings > Subtitles.",
    "Library titles now request missing covers independently of the Board, using metadata addons with bounded requests and profile isolation.",
    "Authored ASS and bitmap subtitles keep their existing behavior. This test build includes both fixes; the current stable release remains 1.0.0.3."
};
}
