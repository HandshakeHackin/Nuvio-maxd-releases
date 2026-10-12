// Adapted from LoZazaMastro/Stremio-Plus at
// 64d1bfb5b78b3c6a37e013973402f3239bee5df0; see THIRD_PARTY.md.
// Stremio - follow the PS5's current output resolution before opening EGL.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

// -1 follows the console; 0/1/2 explicitly request 1080p/1440p/2160p.
constexpr int kDisplayOutputAutomatic = -1;
int display_output_preference(std::string_view stored);
const char* display_output_preference_name(int preference);

struct DisplayOutputSelection {
    int width = 1920, height = 1080;
    uint32_t configured_width = 0, configured_height = 0;
    uint32_t pane_width = 0, pane_height = 0;
    uint64_t refresh_code = 0;
    int open_result = -1, query_result = -1, close_result = 0;
    bool automatic = true;
    bool fallback = false;
    // A failed close cannot safely be followed by a second VideoOut open.
    bool safe_to_open = true;
    const char* fallback_reason = "none";
};

// Call once before EGL initialization. The native probe only opens, reads and
// closes VideoOut: it never configures the console mode or registers buffers.
// The caller must log the actual EGL surface size after opening the display.
DisplayOutputSelection display_output_select(int preference, bool dynamic_modes = true);
