#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "IPTV Library: a separate movie/show tab appears when your imported provider offers VOD. Includes categories, search and private resume.",
    "EVO 0.13.0, Native 10-bit VP9 support and optimized GPU video uploads. Console testing is needed.",
    "IPTV provider error guidance, retained saved catalogs and channels listed in every provider category.",
    "Updated build tools, text layout and best-effort addon manifest refresh. Four-screen TV and audio/sync controls remain available."
};
}
