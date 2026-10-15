#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
read -r -a flags <<< "${HOST_PCM_CFLAGS:-$(pkg-config --cflags libswresample libavutil)}"
read -r -a libs <<< "${HOST_PCM_LIBS:-$(pkg-config --libs libswresample libavutil)}"
${HOST_CXX:-g++} -std=c++20 -Wall -Wextra -Werror -I"$ROOT/src" \
    "${flags[@]}" "$ROOT/tests/audio_pcm_host.cpp" \
    "${libs[@]}" -o "$ROOT/test-output/audio-pcm-host"
"$ROOT/test-output/audio-pcm-host"
