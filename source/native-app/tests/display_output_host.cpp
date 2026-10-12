// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from LoZazaMastro/Stremio-Plus at
// 64d1bfb5b78b3c6a37e013973402f3239bee5df0; see THIRD_PARTY.md.
// Exercise the production output selector with fake VideoOut imports.
// No EGL, console, networking, or system output changes are involved.
#include "display_output.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
struct Status {
    uint32_t full_width, full_height, pane_width, pane_height;
    uint64_t refresh_rate;
    float screen_size_inches;
    uint32_t reserved[4];
};
static_assert(sizeof(Status) == 48, "Fake VideoOut must use the observed 48-byte ABI");
Status status{};
int open_result = 37, query_result = 0, close_result = 0;
bool bad_arguments = false;
std::string calls;
std::vector<std::string> logs;
void reset(uint32_t width = 3840, uint32_t height = 2160) {
    status = {width, height, 1920, 1080, 2, 55.0f, {}};
    open_result = 37; query_result = close_result = 0;
    calls.clear(); logs.clear();
}
bool log_contains(const std::string& value) {
    for (const auto& log : logs)
        if (log.find(value) != std::string::npos) return true;
    return false;
}
}  // namespace

extern "C" int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void* parameter) {
    calls += 'O';
    if (user_id != 0xff || bus_type != 0 || index != 0 || parameter != nullptr) bad_arguments = true;
    return open_result;
}
extern "C" int sceVideoOutGetResolutionStatus(int32_t handle, void* output) {
    calls += 'Q';
    if (handle != open_result || !output) bad_arguments = true;
    if (output) std::memcpy(output, &status, sizeof(status));
    return query_result;
}
extern "C" int sceVideoOutClose(int32_t handle) {
    calls += 'C';
    if (handle != open_result) bad_arguments = true;
    return close_result;
}
void dlog(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    logs.emplace_back(buffer);
}

int main() {
    try {
        expect(display_output_preference("ps5") == -1 && display_output_preference("") == -1 &&
               display_output_preference("invalid") == -1,
               "missing or invalid new preference uses the automatic migration default");
        expect(display_output_preference("1080p") == 0 && display_output_preference("1440p") == 1 &&
               display_output_preference("2160p") == 2,
               "only explicit new manual preference strings select manual output modes");
        for (int preference = -1; preference < 3; ++preference)
            expect(display_output_preference(display_output_preference_name(preference)) == preference,
                   "stored automatic and manual preferences round-trip without index migration ambiguity");
        expect(std::string(display_output_preference_name(99)) == "ps5",
               "invalid in-memory preference serializes safely as automatic");

        const int widths[] = {1920, 2560, 3840};
        const int heights[] = {1080, 1440, 2160};
        for (int mode = 0; mode < 3; ++mode) {
            reset(uint32_t(widths[mode]), uint32_t(heights[mode]));
            const auto output = display_output_select(-1);
            expect(output.width == widths[mode] && output.height == heights[mode] &&
                   output.automatic && !output.fallback && output.safe_to_open,
                   "automatic output follows each supported full PS5 resolution");
            expect(calls == "OQC" && output.open_result == 37 && output.query_result == 0 && output.close_result == 0,
                   "temporary VideoOut handle is queried and closed exactly once before selection returns");
        }
        expect(log_contains("configured=3840x2160 pane=1920x1080") && log_contains("requested=3840x2160"),
               "diagnostic log distinguishes configured full output, pane and selected render size");

        reset(); open_result = -77;
        auto output = display_output_select(-1);
        expect(output.width == 1920 && output.height == 1080 && output.fallback && output.safe_to_open &&
               std::string(output.fallback_reason) == "videoout_open_failed" && calls == "O",
               "failed probe open uses 1080p and does not query or close an invalid handle");
        reset(); query_result = -88;
        output = display_output_select(-1);
        expect(output.width == 1920 && output.height == 1080 && output.fallback && output.safe_to_open &&
               std::string(output.fallback_reason) == "resolution_query_failed" && calls == "OQC",
               "failed resolution query ignores partial output data and still closes its handle");
        reset(0, 0);
        output = display_output_select(-1);
        expect(output.width == 1920 && output.fallback && calls == "OQC",
               "empty full resolution fails safely instead of substituting cropped pane geometry");
        reset(3840, 1440);
        output = display_output_select(-1);
        expect(output.width == 1920 && output.height == 1080 && output.fallback &&
               std::string(output.fallback_reason) == "unsupported_resolution",
               "unsupported aspect ratio is not rounded into a different console resolution");
        reset(std::numeric_limits<uint32_t>::max(), 2160);
        output = display_output_select(-1);
        expect(output.width == 1920 && output.fallback && output.configured_width == std::numeric_limits<uint32_t>::max(),
               "invalid large status dimensions remain intact for diagnosis and cannot choose a render size");

        reset(); close_result = -99;
        output = display_output_select(-1);
        expect(!output.safe_to_open && std::string(output.fallback_reason) == "videoout_close_failed" && calls == "OQC",
               "failed probe close prevents opening a second VideoOut through EGL");
        reset(); open_result = 0;
        output = display_output_select(-1);
        expect(output.width == 3840 && output.safe_to_open && calls == "OQC",
               "handle zero is valid and is still queried and closed");

        reset(1920, 1080);
        output = display_output_select(2);
        expect(output.width == 3840 && output.height == 2160 && !output.automatic && !output.fallback &&
               output.configured_width == 1920 && calls == "OQC",
               "explicit new manual override remains distinct from configured console output");
        reset(); query_result = -88;
        output = display_output_select(1);
        expect(output.width == 2560 && output.height == 1440 && !output.automatic && !output.fallback && output.safe_to_open,
               "manual mode does not require a successful resolution query but the probe is released");
        reset();
        output = display_output_select(-1, false);
        expect(output.width == 1920 && output.height == 1080 && output.fallback &&
               std::string(output.fallback_reason) == "runtime_modes_unavailable",
               "SDK without runtime display modes reports its explicit 1080p fallback");
        reset(1920, 1080);
        output = display_output_select(-1, false);
        expect(output.width == 1920 && !output.fallback,
               "fixed 1080p SDK is already correct when the console reports 1080p");
        reset(2560, 1440);
        output = display_output_select(99);
        expect(output.width == 2560 && output.automatic && !output.fallback,
               "invalid in-memory mode follows the console without out-of-bounds indexing");
        expect(!bad_arguments, "all real helper calls use the observed VideoOut open/query/close ABI");
        std::cout << "PS5 display output: " << checks << " display output selection and probe-lifecycle assertions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " assertions: " << error.what() << '\n';
        return 1;
    }
}
