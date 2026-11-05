#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,4> release_note_items = {
    "1.0.0.11: provider HTML selectors now use maintained Cheerio, fixing the known nth-check denial-of-service advisory while preserving provider module names.",
    "Updated subtitle shaping with HarfBuzz 15.1.0, artwork handling with WebP 1.6.0 and XML parsing with libxml2 2.15.4. Existing FFmpeg, EVO and graphics release lines are retained.",
    "Includes Plex, Jellyfin and Emby, authenticated library artwork and saved connection fixes, optional torrent streaming and paired Web Drop, plus HDR-to-SDR fallback.",
    "Remote button mapping remains experimental and app-local: PlayStation or your TV may intercept buttons before Nuvio receives them. Controller controls stay unchanged. Console testing is still required for updated libraries and experimental playback features."
};
}
