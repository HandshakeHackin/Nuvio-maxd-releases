#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "Test build: large Live TV imports support up to 100,000 channels and 128 MiB; Xtream reads live channels only.",
    "Experimental four-screen Live TV: choose four SDR channels, use D-pad to focus and Cross to listen. SD recommended.",
    "Audio volume/boost and audio/subtitle sync controls are in Settings and the playback track menu. Boost uses PCM.",
    "Four-screen mode needs four provider connections. Console performance and compatibility need testing."
};
}
