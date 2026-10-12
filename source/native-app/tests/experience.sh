#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
 -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$ROOT/gl" -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/third_party/hui/src" \
 "$ROOT/tests/experience_host.cpp" "$ROOT/ui/player.cpp" "$ROOT/gl/episode_playback.cpp" \
 "$ROOT/third_party/hui/src/gfx/font.cpp" "$ROOT/third_party/hui/src/gfx/draw_list.cpp" \
 "$ROOT/src/diagnostics.cpp" "$ROOT/src/util.cpp" "$ROOT/src/stremio.cpp" -o "$ROOT/test-output/experience-host"
"$ROOT/test-output/experience-host"
