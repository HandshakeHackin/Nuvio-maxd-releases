#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -DGL_GLEXT_PROTOTYPES=1 \
    -I"$HUI/src" -I"$ROOT/tests/display_sdk_fixture" \
    "$ROOT/tests/display_refresh_host.cpp" "$HUI/src/platform/ps5/display_egl.cpp" \
    -o "$ROOT/test-output/display-refresh-host"
"$ROOT/test-output/display-refresh-host"
