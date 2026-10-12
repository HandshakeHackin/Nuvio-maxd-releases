#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
EVO="$ROOT/third_party/evo"
mkdir -p "$ROOT/test-output"
read -r -a cflags <<< "${HOST_LIFECYCLE_CFLAGS:-$(pkg-config --cflags libavcodec libavformat libavutil libass libswresample libswscale fribidi)}"
read -r -a libs <<< "${HOST_LIFECYCLE_LIBS:-$(pkg-config --libs libavcodec libavformat libavutil libass libswresample libswscale fribidi libcurl)}"
cat > "$ROOT/test-output/lifecycle.srt" <<'SUB'
1
00:00:01,000 --> 00:00:11,000
Embedded captions
SUB
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
    -f lavfi -i 'sine=frequency=440:sample_rate=48000' -i "$ROOT/test-output/lifecycle.srt" -t 12 -c:v libx264 -preset ultrafast -bf 0 -g 48 -c:a aac -c:s mov_text -metadata:s:s:0 language=eng -disposition:s:0 default \
    "$ROOT/test-output/lifecycle.mp4"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -Wno-misleading-indentation -pthread -DHAVE_FRIBIDI -DPLATFORM_PS5_NATIVE=1 \
    -I"$ROOT/src" -I"$ROOT/third_party" -I"$EVO/media/include" -I"$EVO/pp/include" -I"$EVO/include" "${cflags[@]}" \
    "$ROOT/tests/player_lifecycle_host.cpp" "$ROOT/src/player.cpp" "$ROOT/src/pcm_out.cpp" "$ROOT/src/packet_storage.cpp" \
    "$ROOT/src/subtitle_renderer.cpp" "$ROOT/src/subtitles.cpp" "$ROOT/src/video_frame.cpp" "$ROOT/src/yuv_convert.cpp" \
    "$ROOT/src/util.cpp" "$ROOT/src/bidi.cpp" "${libs[@]}" -o "$ROOT/test-output/player-lifecycle-host"
"$ROOT/test-output/player-lifecycle-host" "$ROOT/test-output/lifecycle.mp4"
