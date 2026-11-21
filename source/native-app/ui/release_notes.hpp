#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.1.0: personal IPTV and enabled free channels share one unified TV guide.",
    "Choose multiple countries for free TV. Availability and quality depend on third-party providers.",
    "Channels keep their own source, guide, favorites and playback details.",
    "Open Settings > Live TV > Unified TV guide. Individual sources and four-screen TV remain available.",
};
}
