#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
EVO="$ROOT/third_party/evo"
OUT="$ROOT/test-output/evo"
mkdir -p "$OUT"
python3 "$ROOT/tools/validate_evo_source.py"
read -r -a flags <<< "${HOST_FFMPEG_CFLAGS:-$(pkg-config --cflags libavcodec libavformat libavutil libswscale)}"
read -r -a libs <<< "${HOST_EVO_FFMPEG_LIBS:-$(pkg-config --libs libavcodec libavformat libavutil libswscale)}"
includes=(-I"$ROOT/src" -I"$ROOT/third_party" -I"$EVO/media/include" -I"$EVO/media/src" -I"$EVO/pp/include" -I"$EVO/include")
objs=()
for file in "$EVO/media/src/evo_vdec_ffmpeg.c" "$EVO/media/src/evo_vdec_native.c" \
            "$EVO/pp/src/pp_clock.c" "$EVO/pp/src/pp_playback.c"; do
    obj="$OUT/$(basename "$file").o"
    ${HOST_CC:-gcc} -std=gnu11 -O1 -g -Wall -Wextra "${flags[@]}" "${includes[@]}" -c "$file" -o "$obj"
    objs+=("$obj")
done
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
    -frames:v 24 -c:v libx264 -preset medium -bf 2 -g 12 -pix_fmt yuv420p "$OUT/sdr.mp4"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
    -frames:v 24 -c:v libx265 -preset fast -x265-params 'pools=1:frame-threads=1:log-level=error' \
    -pix_fmt yuv420p10le -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc "$OUT/hdr.mp4"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    "${flags[@]}" "${includes[@]}" "$ROOT/tests/evo_host.cpp" \
    "$ROOT/src/evo_backend.cpp" "$ROOT/src/evo_runtime.cpp" "$ROOT/src/video_frame.cpp" \
    "${objs[@]}" "${libs[@]}" -o "$OUT/evo-host"
"$OUT/evo-host" "$OUT/sdr.mp4" "$OUT/hdr.mp4"
