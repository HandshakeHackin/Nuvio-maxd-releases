#pragma once
#include <array>
#include <string>
#include <string_view>
namespace sx {
inline bool should_show_release_notes(std::string_view seen, std::string_view current) {
    return !current.empty() && seen != current;
}
inline constexpr std::array<const char*,6> release_note_items = {
    "Updated the graphics driver to OpenGL SDK 1.0.2.",
    "Periodic driver logging is opt-in to avoid interrupting playback.",
    "Includes GPU cache, shader-query and submission timeout fixes.",
    "Stream reloads keep your selection while addons finish responding.",
    "Added startup-stage timings to help diagnose slow or failed launches.",
    "HDR, 120 Hz and optional bitstream audio remain experimental."
};
}
