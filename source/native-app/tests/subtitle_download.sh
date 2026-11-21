#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/subtitle_download_host.cpp" \
    "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" $(pkg-config --cflags --libs libcurl) \
    -Wl,--wrap=curl_easy_perform -o "$ROOT/test-output/subtitle-download-host"
python3 "$ROOT/tests/subtitle_download_test.py"
