#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ROOT/ui" -I"$HUI/src" "$ROOT/tests/settings_categories_host.cpp" \
    "$ROOT/ui/settings.cpp" "$ROOT/ui/player.cpp" "$ROOT/ui/chrome.cpp" "$ROOT/ui/page.cpp" "$ROOT/ui/cards.cpp" \
    "$HUI/src/gfx/font.cpp" "$HUI/src/gfx/draw_list.cpp" "$HUI/src/ui/glyphs.cpp" \
    -o "$ROOT/test-output/settings-categories-host"
"$ROOT/test-output/settings-categories-host"
