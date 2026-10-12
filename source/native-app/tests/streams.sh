#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ROOT/ui" -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party" \
    -I"$ROOT/third_party/hui/src" -I"$ROOT/third_party/hui/third_party/stb" \
    "$ROOT/tests/streams_host.cpp" "$ROOT/ui/detail.cpp" \
    "$ROOT/src/stremio.cpp" "$ROOT/src/util.cpp" "$ROOT/third_party/hui/src/gfx/font.cpp" \
    -o "$ROOT/test-output/streams-host"
"$ROOT/test-output/streams-host"
