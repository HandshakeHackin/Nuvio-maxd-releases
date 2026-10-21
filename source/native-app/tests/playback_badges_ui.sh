#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$ROOT/ui" -I"$HUI/src" "$ROOT/tests/playback_badges_ui_host.cpp" \
 "$ROOT/ui/player.cpp" "$ROOT/ui/cards.cpp" "$ROOT/ui/chrome.cpp" \
 "$HUI/src/gfx/font.cpp" "$HUI/src/gfx/draw_list.cpp" "$HUI/src/gfx/triangulate.cpp" "$HUI/src/ui/glyphs.cpp" \
 -o "$ROOT/test-output/playback-badges-ui-host"
"$ROOT/test-output/playback-badges-ui-host"
