#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Live TV adds M3U and Xtream Codes sources, channel groups, search, favorites and recent channels.",
    "Anime skip buttons add AniSkip and optional Anime-Skip, with safer episode and release matching.",
    "Watch trailers from title details with Options. Search settings; recent eligible sources appear while refreshing.",
    "Audio switching keeps your place. Recent sources load while refreshing; Trakt, provider and updater fixes retained."
};
}
