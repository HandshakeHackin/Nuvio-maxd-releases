#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Choose, import or create up to six Nuvio account profiles.",
    "Separate Watch Progress, history and local provider switches per profile.",
    "Sync addons and plugin repositories; respect inherited primary sources.",
    "Provider menus scroll with your selection and stay open when toggling.",
    "Provider downloads use shared IPv4 setup and show specific errors.",
    "Profile PIN checks and account sync still need real-account PS5 testing."
};
}
