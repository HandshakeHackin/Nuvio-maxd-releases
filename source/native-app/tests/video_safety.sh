#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
GL=${GL_SDK:-/usr}
[ -f "$GL/include/GL/glcorearb.h" ] || { echo "Set GL_SDK to the pinned OpenGL SDK for video safety checks" >&2; exit 1; }
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -I"$ROOT/gl" "$ROOT/tests/scanout_driver_host.cpp" -o "$ROOT/test-output/scanout-driver-host"
"$ROOT/test-output/scanout-driver-host"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DGL_GLEXT_PROTOTYPES=1 \
    -I"$GL/include" -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party/hui/src" \
    "$ROOT/tests/video_surface_lifetime_host.cpp" "$ROOT/gl/video_surface.cpp" -Wl,--gc-sections -o "$ROOT/test-output/video-surface-lifetime-host"
"$ROOT/test-output/video-surface-lifetime-host"
