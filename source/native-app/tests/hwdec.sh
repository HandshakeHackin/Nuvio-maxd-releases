#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
read -r -a ffmpeg_cflags <<< "${HOST_FFMPEG_CFLAGS:-$(pkg-config --cflags libavcodec libavutil)}"
read -r -a ffmpeg_libs <<< "${HOST_FFMPEG_LIBS:-$(pkg-config --libs libavcodec libavutil)}"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "${ffmpeg_cflags[@]}" \
    "$ROOT/tests/hwdec_host.cpp" "${ffmpeg_libs[@]}" -o "$ROOT/test-output/hwdec-host"
"$ROOT/test-output/hwdec-host"
