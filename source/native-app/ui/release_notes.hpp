#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.20: encrypted configuration backup, TheTVDB episode ordering and Stalker/Ministra portals.",
    "Settings > Configuration backup and sync saves encrypted local backups and supports manual Google Drive sync with conflict review. Drive requires a suitable OAuth app registration.",
    "Set your own TVDB API key in Settings, then choose Episode order from a series title's Options menu. Verified mappings keep history and resume identities unchanged.",
    "Live TV > Options can add a provider-authorised Stalker/Ministra portal, including live channels, guide and available movies/series. These features await PS5/provider confirmation.",
};
}
