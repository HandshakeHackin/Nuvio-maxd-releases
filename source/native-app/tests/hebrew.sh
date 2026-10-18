#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
read -r -a flags <<< "${HOST_HEBREW_CFLAGS:-$(pkg-config --cflags fribidi)}"
read -r -a libs <<< "${HOST_HEBREW_LIBS:-$(pkg-config --libs fribidi)}"
${HOST_CXX:-g++} -std=c++20 -O1 -ffunction-sections -fdata-sections -DHAVE_FRIBIDI \
 -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party/hui/src" -I"$ROOT/third_party/hui/third_party/stb" "${flags[@]}" \
 "$ROOT/tests/hebrew_host.cpp" "$ROOT/ui/dynamic_font.cpp" "$ROOT/src/bidi.cpp" "$ROOT/third_party/hui/src/core/save_file.cpp" \
 -Wl,--gc-sections "${libs[@]}" -o "$ROOT/test-output/hebrew-host"
"$ROOT/test-output/hebrew-host" "$ROOT/app/fonts"
