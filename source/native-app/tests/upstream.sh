#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party/hui/src" "$ROOT/tests/upstream_host.cpp" \
    "$ROOT/third_party/hui/src/gfx/font.cpp" -o "$ROOT/test-output/upstream-host"
"$ROOT/test-output/upstream-host"
