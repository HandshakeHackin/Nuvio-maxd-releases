#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,5> release_note_items = {
    "Provider metadata access and playback headers corrected; affected German providers need PS5 retesting.",
    "Added provider options per profile and clearer metadata connection errors.",
    "Stream loading progress stays visible; recent eligible results appear while sources refresh.",
    "Trakt phone pairing now uses Max'd's registered application, without developer credentials.",
    "Includes the updater handoff safeguards and earlier playback fixes from test build 0.5.34.2."
};
}
