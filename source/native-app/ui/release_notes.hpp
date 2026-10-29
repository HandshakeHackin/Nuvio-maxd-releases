#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "IPTV Library: browse A-Z and search saved movie/show catalogs across your profile's providers, with provider-labelled results.",
    "Embedded chapters: use Options/Up, select Chapters, and press Cross to jump. Available with supported Native, EVO and VLC files.",
    "Better playback recovery preserves audio/subtitle choices, sync and volume. Missing HEVC colour tags can be recovered from codec metadata.",
    "Bounded artwork queues, updated subtitle/network libraries and a startup compatibility adjustment. Physical PS5 testing is still needed."
};
}
