#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -DHAVE_FRIBIDI -pthread -I"$ROOT/src" -I"$ROOT/third_party" \
 "$ROOT/tests/thai_subtitles_host.cpp" "$ROOT/src/subtitle_renderer.cpp" "$ROOT/src/subtitles.cpp" "$ROOT/src/video_frame.cpp" "$ROOT/src/bidi.cpp" "$ROOT/src/util.cpp" \
 $(pkg-config --cflags --libs libavcodec libavutil libass fribidi harfbuzz libcurl) -o "$ROOT/test-output/thai-subtitles-host"
"$ROOT/test-output/thai-subtitles-host" "$ROOT/app/fonts"

python3 "$ROOT/tests/thai_font_pins.py"
python3 - "$ROOT/test-output/thai-subtitles-host" "$ROOT/app/fonts" <<'PYCODEC'
import subprocess,sys
actual=subprocess.check_output([sys.argv[1],sys.argv[2],'--cp874-vector'])
assert actual==bytes(range(256)).decode('cp874',errors='replace').encode('utf-8')
print('Thai legacy decoding: all 256 CP874 bytes match the independent Python codec')
PYCODEC
