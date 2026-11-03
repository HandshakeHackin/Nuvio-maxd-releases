#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
python3 - "$ROOT" <<'EVO_FILTER'
from pathlib import Path
import sys
r=Path(sys.argv[1]);s=(r/'third_party/evo/media/src/evo_vdec_native.c').read_text()
a=s.index('__attribute__((noinline)) static int hevc_copy_base_layer(')
b=s.index('{',a);end=b+1;depth=1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}');end+=1
assert '#define AU_MAX_NALS 512' in s
(r/'test-output/evo_hevc_filter.hpp').write_text('#define AU_MAX_NALS 512\n'+s[a:end].replace('hevc_copy_base_layer','evo_hevc_copy_base_layer')+'\n')
EVO_FILTER
read -r -a ffmpeg_cflags <<< "${HOST_FFMPEG_CFLAGS:-$(pkg-config --cflags libavcodec libavutil)}"
read -r -a ffmpeg_libs <<< "${HOST_FFMPEG_LIBS:-$(pkg-config --libs libavcodec libavutil)}"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/test-output" -I"$ROOT/src" -I"$ROOT/third_party" "${ffmpeg_cflags[@]}" \
    "$ROOT/tests/hwdec_host.cpp" "${ffmpeg_libs[@]}" -o "$ROOT/test-output/hwdec-host"
"$ROOT/test-output/hwdec-host"
