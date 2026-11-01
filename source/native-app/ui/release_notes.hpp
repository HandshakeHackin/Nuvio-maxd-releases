#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Library search: Triangle/Options or the Search filter finds titles in your current profile and Library source. Type and sort filters still apply.",
    "Live TV now/next: provider programme listings on channel cards. Xtream and XMLTV guides are supported; Options has guide URL, refresh and on/off controls.",
    "New-episode badges: Library series show aired updates when synchronized watched history and matching episode metadata are available.",
    "Reporter-confirmed fixes: larger external subtitles with 150%/200% choices and independent Trakt Library covers. New features still need console testing."
};
}
