#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 "$ROOT/tests/tracker_applications_test.py"
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/tracking_host.cpp" \
    "$ROOT/src/tracking.cpp" "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" \
    $(pkg-config --cflags --libs openssl libcurl) -o "$ROOT/test-output/tracking-host"
"$ROOT/test-output/tracking-host"
