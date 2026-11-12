#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.21 experimental: MDBList library sync fix, awaiting tester confirmation.",
    "MDBList add/remove confirmations now support scalar and per-media counts independently of Trakt.",
    "Pending changes check the remote watchlist before retrying. Watchlist pagination and ID-format compatibility are bounded and validated.",
    "Please confirm movie/series add and remove on mdblist.com, including after restart. Profile data, watched history and resume positions remain separate.",
};
}
