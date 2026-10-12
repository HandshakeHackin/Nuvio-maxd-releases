#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Experimental HDR switching now runs inside the graphics driver.",
    "More detailed HDR switch and restore results for console testing.",
    "Optional Auto and Bitstream audio output in Settings.",
    "Dolby Digital, Dolby Digital Plus and DTS core passthrough with PCM fallback.",
    "TrueHD and DTS-HD keep decoded lossless PCM; PCM remains the default.",
    "Reopen these notes from Settings → What's new."
};
}
