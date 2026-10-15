#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party" \
    "$ROOT/tests/presentation_timing_host.cpp" "$ROOT/gl/presentation_timing.cpp" \
    -Wl,--wrap=clock_gettime -o "$ROOT/test-output/presentation-timing-host"
"$ROOT/test-output/presentation-timing-host"
