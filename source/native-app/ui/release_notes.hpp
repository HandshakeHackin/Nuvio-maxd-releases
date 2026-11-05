#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.8 experimental: private media artwork now loads through authenticated workers; saved connections are read back and verified. Password keyboards use masked Basic Latin input for firmware compatibility. Connect Jellyfin, Emby or Plex under Account & Services > Media servers. Browse, search, resume and report server watch progress separately for each profile.",
    "Optional PCM Night Mode and stereo bass levels, provider guide-clock correction, chapter skip buttons, trailer language preference and Native/EVO subtitle auto-sync are included.",
    "Updated FFmpeg 7.0.3 and XML/image/font libraries, with additional subtitle safety fixes. Native/EVO now omit Dolby Vision enhancement pictures while preserving compatible HDR10 base video.",
    "Confirmed subtitle-size and Trakt-cover fixes remain included. Real-console/server testing is still needed. Plex: scan the QR code, approve at plex.tv/link and choose your server."
};
}
