#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -pthread -Wall -Wextra -Werror \
 -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/iptv_library_host.cpp" "$ROOT/src/iptv_library.cpp" "$ROOT/src/live_tv.cpp" \
 $(pkg-config --cflags --libs libcurl) -o "$ROOT/test-output/iptv-library-host"
TASK_IPTV_TMP=$(mktemp -d)
trap 'rm -rf "$TASK_IPTV_TMP"' EXIT
"$ROOT/test-output/iptv-library-host" "$TASK_IPTV_TMP"
