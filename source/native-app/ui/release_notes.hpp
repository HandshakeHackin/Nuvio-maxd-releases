#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.17: English and German menus. Choose Language in Settings, then restart Max'd. Audio, subtitle and addon language choices remain separate.",
    "Cast and crew photos: press R2 on a title page. Select a person to read their biography and browse movie and TV credits. Circle returns to the previous view.",
    "Credits open titles through your configured addons. Missing metadata or photos use fallbacks; browsing keeps profile data isolated.",
    "Keep playback, profiles and saved USB playlists when updating. Fully close Max'd first; do not overwrite a mounted image."
};
}
