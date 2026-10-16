#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
read -r -a ffmpeg_cflags <<< "${HOST_FFMPEG_CFLAGS:-$(pkg-config --cflags libavformat libavutil)}"
read -r -a ffmpeg_libs <<< "${HOST_AVFORMAT_LIBS:-$(pkg-config --libs libavformat libavutil)}"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "${ffmpeg_cflags[@]}" \
    "$ROOT/tests/netstream_host.cpp" "$ROOT/src/netstream.cpp" "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" \
    $(pkg-config --cflags --libs libcurl) "${ffmpeg_libs[@]}" \
    -Wl,--wrap=curl_easy_perform -Wl,--wrap=curl_easy_init -o "$ROOT/test-output/netstream-host"
python3 "$ROOT/tests/netstream_test.py"
