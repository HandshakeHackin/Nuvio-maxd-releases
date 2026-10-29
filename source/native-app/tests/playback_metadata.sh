#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'color=size=128x72:rate=1' -frames:v 1 \
 -c:v libx265 -preset ultrafast -x265-params 'pools=1:frame-threads=1:log-level=error:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc' \
 -f hevc "$ROOT/test-output/sps-colour.hevc"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'color=size=128x72:rate=1' -frames:v 1 \
 -pix_fmt yuv420p10le -c:v libx265 -preset ultrafast \
 -x265-params 'pools=1:frame-threads=1:log-level=error:colorprim=bt709:transfer=bt709:colormatrix=bt709' \
 -f hevc "$ROOT/test-output/sps-sdr10.hevc"
read -r -a flags <<< "${HOST_LIFECYCLE_CFLAGS:-$(pkg-config --cflags libavcodec)}"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/src" "${flags[@]}" "$ROOT/tests/playback_metadata_host.cpp" -o "$ROOT/test-output/playback-metadata-host"
"$ROOT/test-output/playback-metadata-host" "$ROOT/test-output/sps-colour.hevc" "$ROOT/test-output/sps-sdr10.hevc"
