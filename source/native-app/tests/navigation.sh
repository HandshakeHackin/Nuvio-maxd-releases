#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" \
    "$ROOT/tests/navigation_host.cpp" "$ROOT/ui/board.cpp" \
    "$ROOT/ui/addons.cpp" "$ROOT/ui/chrome.cpp" -o "$ROOT/test-output/navigation-host"
"$ROOT/test-output/navigation-host"
