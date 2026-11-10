#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.15 experimental: USB scanning tries directory enumeration when metadata is unavailable, with private per-mount diagnostics. This detection fix is unconfirmed on PS5. Live TV sources & filters adds Import playlist from USB and Replace current USB playlist. Private copies survive USB removal.  Up / Options, then Square opens playback options for source changes, built-in torrent file selection and live subtitle appearance.",
    "Choose another source for the same title while retaining position. Different torrent files use their own saved progress. Audio delay now has 0.10 / 0.20 second adjustments and reset.",
    "Live TV sources & filters now offers an optional channel/time guide. Browse available listings and tune live channels; missing listings do not prevent playback. Stream details distinguish reported bitrate from measured input rate.",
    "Test build: console confirmation is pending. Also includes optional Sports Hub, source preferences and torrent experience updates. Keep your previous installer and saved user data."
};
}
