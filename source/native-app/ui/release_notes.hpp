#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.26: age ratings on movie and series detail pages.",
    "Choose Age-rating country in Settings > Account & Services. Saved separately for each profile.",
    "The badge names its country. US is the fallback; missing ratings do not create a badge.",
    "Ratings are display only. Eight menu languages and previous playback, library and watched fixes are retained.",
};
}
