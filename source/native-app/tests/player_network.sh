#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
EVO="$ROOT/third_party/evo"
mkdir -p "$ROOT/test-output/player-network"
read -r -a cflags <<< "${HOST_LIFECYCLE_CFLAGS:-$(pkg-config --cflags libavcodec libavformat libavutil libass libswresample libswscale fribidi)}"
read -r -a libs <<< "${HOST_LIFECYCLE_LIBS:-$(pkg-config --libs libavcodec libavformat libavutil libass libswresample libswscale fribidi libcurl)}"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -Wno-misleading-indentation -pthread -DHAVE_FRIBIDI -DPLATFORM_PS5_NATIVE=1 -DNUVIO_HOST_BITSTREAM_API=1 \
    -I"$ROOT/src" -I"$ROOT/third_party" -I"$EVO/media/include" -I"$EVO/pp/include" -I"$EVO/include" "${cflags[@]}" \
    "$ROOT/tests/player_network_host.cpp" "$ROOT/src/player.cpp" "$ROOT/src/vlc_backend.cpp" "$ROOT/src/netstream.cpp" "$ROOT/src/http.cpp" \
    "$ROOT/src/pcm_out.cpp" "$ROOT/src/bitstream.cpp" "$ROOT/src/packet_storage.cpp" \
    "$ROOT/src/subtitle_renderer.cpp" "$ROOT/src/subtitles.cpp" "$ROOT/src/video_frame.cpp" "$ROOT/src/yuv_convert.cpp" \
    "$ROOT/src/util.cpp" "$ROOT/src/bidi.cpp" "${libs[@]}" -Wl,--wrap=avformat_seek_file -Wl,--wrap=av_seek_frame -o "$ROOT/test-output/player-network-host"
python3 "$ROOT/tests/player_network_test.py"
