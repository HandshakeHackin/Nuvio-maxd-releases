#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.18: Menu languages now include English, German, Spanish, French, Arabic, Simplified Chinese, Japanese and Korean.",
    "Choose Language in Settings, then restart Max'd. Some advanced help and provider messages still use English.",
    "Arabic text uses joined letters and right-to-left text order. Chinese, Japanese and Korean use the bundled fonts.",
    "Audio/subtitle preferences and addon content are separate from menu language. Saved playlists and profile data are preserved.",
};
}
