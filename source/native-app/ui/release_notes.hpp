#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.19: Options on movie/series details opens library, watched and trailer actions.",
    "Add or remove library titles with profile sync. Watched actions cover movies and listed released episodes in a season or series.",
    "Bulk watched actions ask for confirmation. Offline changes are saved separately from resume positions.",
    "Episode hints show Triangle marking. Cast and person pages now use controller icons.",
};
}
