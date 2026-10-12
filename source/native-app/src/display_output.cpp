// Adapted from LoZazaMastro/Stremio-Plus at
// 64d1bfb5b78b3c6a37e013973402f3239bee5df0; see THIRD_PARTY.md.
// Stremio - follow the PS5's current output resolution before opening EGL.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_output.h"

#include <cstddef>

#include "util.h"

#ifdef PLATFORM_PS5
namespace {
// ABI observed in the supplied ProsperoLight 01.000.080:
// src/native_agc_present.cpp, video_resolution_status_t and VideoOut imports.
// Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later.
// Full dimensions describe the output; pane dimensions are logged separately
// and must not substitute a possibly cropped pane for the console's output.
struct VideoResolutionStatus {
    uint32_t full_width, full_height;
    uint32_t pane_width, pane_height;
    uint64_t refresh_rate;
    float screen_size_inches;
    uint32_t reserved[4];
};
static_assert(sizeof(VideoResolutionStatus) == 48, "VideoOut resolution status ABI size");
static_assert(alignof(VideoResolutionStatus) == 8, "VideoOut resolution status ABI alignment");
static_assert(offsetof(VideoResolutionStatus, refresh_rate) == 16, "VideoOut refresh ABI offset");
static_assert(offsetof(VideoResolutionStatus, screen_size_inches) == 24, "VideoOut screen ABI offset");
static_assert(offsetof(VideoResolutionStatus, reserved) == 28, "VideoOut reserved ABI offset");
}  // namespace

extern "C" {
int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void* parameter);
int sceVideoOutGetResolutionStatus(int32_t handle, void* status);
int sceVideoOutClose(int32_t handle);
}
#endif

namespace {
constexpr int widths[] = {1920, 2560, 3840};
constexpr int heights[] = {1080, 1440, 2160};

int supported_mode(uint32_t width, uint32_t height) {
    for (int i = 0; i < 3; ++i)
        if (width == uint32_t(widths[i]) && height == uint32_t(heights[i])) return i;
    return -1;
}
}  // namespace

int display_output_preference(std::string_view stored) {
    if (stored == "1080p") return 0;
    if (stored == "1440p") return 1;
    if (stored == "2160p") return 2;
    return kDisplayOutputAutomatic;
}

const char* display_output_preference_name(int preference) {
    constexpr const char* names[] = {"1080p", "1440p", "2160p"};
    return preference >= 0 && preference < 3 ? names[preference] : "ps5";
}

DisplayOutputSelection display_output_select(int preference, bool dynamic_modes) {
    DisplayOutputSelection result;
    result.automatic = preference < 0 || preference > 2;
    const char* probe_error = "platform_unavailable";
#ifdef PLATFORM_PS5
    VideoResolutionStatus status{};
    dlog("Display: probe begin preference=%s", display_output_preference_name(preference));
    result.open_result = sceVideoOutOpen(0xff, 0, 0, nullptr);
    dlog("Display: probe open rc=0x%08x", unsigned(result.open_result));
    if (result.open_result >= 0) {
        result.query_result = sceVideoOutGetResolutionStatus(result.open_result, &status);
        // Close even on a failed query, before returning or opening EGL.
        result.close_result = sceVideoOutClose(result.open_result);
        result.safe_to_open = result.close_result == 0;
        result.configured_width = status.full_width;
        result.configured_height = status.full_height;
        result.pane_width = status.pane_width;
        result.pane_height = status.pane_height;
        result.refresh_code = status.refresh_rate;
        probe_error = result.query_result != 0 ? "resolution_query_failed" : "unsupported_resolution";
    } else {
        probe_error = "videoout_open_failed";
    }
#endif
    const int configured = result.query_result == 0
        ? supported_mode(result.configured_width, result.configured_height) : -1;
    int selected = result.automatic ? configured : preference;
    if (selected < 0) {
        selected = 0;
        result.fallback = true;
        result.fallback_reason = probe_error;
    }
    if (!dynamic_modes && selected != 0) {
        selected = 0;
        result.fallback = true;
        result.fallback_reason = "runtime_modes_unavailable";
    }
    result.width = widths[selected];
    result.height = heights[selected];
    if (!result.safe_to_open) {
        result.fallback = true;
        result.fallback_reason = "videoout_close_failed";
    }
    dlog("Display: configured=%ux%u pane=%ux%u refresh_code=%llu query_rc=0x%08x close_rc=0x%08x "
         "preference=%s requested=%dx%d fallback=%s safe_to_open=%d",
         result.configured_width, result.configured_height, result.pane_width, result.pane_height,
         static_cast<unsigned long long>(result.refresh_code), unsigned(result.query_result),
         unsigned(result.close_result), display_output_preference_name(preference), result.width,
         result.height, result.fallback_reason, result.safe_to_open ? 1 : 0);
    return result;
}
