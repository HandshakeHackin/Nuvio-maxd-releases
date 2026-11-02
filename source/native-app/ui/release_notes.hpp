#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.6 test build: experimental Jellyfin and Emby connections under Account & Services > Media servers. A separate sidebar appears after sign-in.",
    "Browse server movies/episodes, search, resume and report watch progress. Try Compatible playback for server HLS. Server connections stay separate for each Nuvio profile.",
    "Also staged: PCM Night Mode and stereo bass levels, provider guide-clock correction, chapter skip buttons, trailer language preference and optional subtitle auto-sync.",
    "Subtitle reader/hardening updates and reporter-confirmed subtitle-size/Trakt-cover fixes are retained. Physical-console/server testing is pending; please report results in issue #14."
};
}
