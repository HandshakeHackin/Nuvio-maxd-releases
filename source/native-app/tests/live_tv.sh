#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror \
 -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/live_tv_host.cpp" "$ROOT/src/live_tv.cpp" \
 $(pkg-config --cflags --libs libcurl) -o "$ROOT/test-output/live-tv-host"
python3 "$ROOT/tests/live_tv_test.py" "$ROOT/test-output/live-tv-host"
