#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.22 experimental: MDBList watched-history and movie indicator fixes, awaiting console confirmation.",
    "Episode, season and show history now uses validated complete pages. Failed or incomplete reads retain local checks.",
    "Movie watched checks follow history on Home, Library, Discover and Search. Details show a translated Watched badge.",
    "Please test episode checks after revisiting and restarting, plus movie watched/unwatched badges. Resume positions remain unchanged.",
};
}
