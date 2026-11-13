#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.23 experimental: explicit MDBList episode history and steady carousel selection.",
    "Episode checks now follow individual history records. Show and season activity summaries no longer mark other episodes automatically.",
    "The old inferred-mark cache is retired locally. Explicit marks, resume positions and pending changes stay separate.",
    "Marking or refreshing watched state keeps the selected episode and scroll position. Please confirm the fixes on your console.",
};
}
