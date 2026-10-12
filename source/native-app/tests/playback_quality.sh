#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
read -r -a cflags <<< "${HOST_QUALITY_CFLAGS:-$(pkg-config --cflags libavcodec libavutil libass fribidi)}"
read -r -a libs <<< "${HOST_QUALITY_LIBS:-$(pkg-config --libs libavcodec libavutil libass fribidi libcurl)}"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -DPLATFORM_PS5_NATIVE=1 -DHAVE_FRIBIDI \
    -I"$ROOT/src" -I"$ROOT/third_party" "${cflags[@]}" \
    "$ROOT/tests/playback_quality_host.cpp" "$ROOT/src/pcm_out.cpp" "$ROOT/src/packet_storage.cpp" \
    "$ROOT/src/subtitle_renderer.cpp" "$ROOT/src/subtitles.cpp" "$ROOT/src/video_frame.cpp" "$ROOT/src/util.cpp" "$ROOT/src/bidi.cpp" \
    "${libs[@]}" -o "$ROOT/test-output/playback-quality-host"
"$ROOT/test-output/playback-quality-host" "$ROOT/app/fonts"
