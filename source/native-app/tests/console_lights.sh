#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -pthread \
    -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party" \
    "$ROOT/tests/console_lights_host.cpp" "$ROOT/gl/console_lights.cpp" \
    -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl -ldl -o "$ROOT/test-output/console-lights-host"
"$ROOT/test-output/console-lights-host"
