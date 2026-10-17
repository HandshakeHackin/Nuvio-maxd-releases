#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Add Nuvio provider/plugin repositories by manifest URL.",
    "Enable individual providers on the Addons page; standard addons still work.",
    "Media Remote and HDMI-CEC navigation and playback controls added.",
    "Choose 30 seconds to 5 minutes of Native/EVO read-ahead in Playback settings.",
    "Title details now load ahead of background requests; leaving a title cancels its search.",
    "Provider compatibility and remote controls need PS5 testing; Native stays the default."
};
}
