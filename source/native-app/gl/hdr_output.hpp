// VideoOut adapter for the pinned OpenGL SDK's presentation buffers.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>

namespace sx {
class HdrOutput {
public:
    // Must run between frames, after glFinish(). Changes take effect at flip.
    static bool set(bool hdr, int width, int height);
    static void presented();
    static bool bgr(); // successful candidate determines RGB/BGR word packing
    static std::uint64_t format();
    static std::string status();
    static bool active(); // pixels must use the 10-bit layout, including while pending
    static bool confirmed(); // VideoOut reports HDR after a successful flip
    static bool failed();
    static std::uint32_t result();
    static std::string error(); // failure stage and native result, for player logs/UI
};
}
