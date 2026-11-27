#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Standalone PS5 ports for 20 German providers, including KellerKino. Enable them from Addons.",
    "CloudStream repositories label providers that do not have a PS5 port.",
    "Unavailable sources stay visible with retry and remove options. Saved source metadata is preserved.",
    "Includes the previous subtitle fixes and optional USB torrent cache.",
};
}
