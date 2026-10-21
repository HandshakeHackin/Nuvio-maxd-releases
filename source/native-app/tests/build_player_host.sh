#!/usr/bin/env bash
# Shared, dependency-checked objects for the three production-player fixtures.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output=${1:?output name}; main=${2:?fixture source}; mode=${3:-file}; wrap=${4:-none}
EVO="$ROOT/third_party/evo"
read -r -a cflags <<< "${HOST_LIFECYCLE_CFLAGS:-$(pkg-config --cflags libavcodec libavformat libavutil libass libswresample libswscale fribidi)}"
read -r -a libs <<< "${HOST_LIFECYCLE_LIBS:-$(pkg-config --libs libavcodec libavformat libavutil libass libswresample libswscale fribidi libcurl)}"
CXX=${HOST_CXX:-g++}
flags=(-std=c++20 -O0 -g -Wall -Wextra -Werror -Wno-misleading-indentation -pthread
 -DHAVE_FRIBIDI -DPLATFORM_PS5_NATIVE=1 -DNUVIO_HOST_BITSTREAM_API=1
 -I"$ROOT/src" -I"$ROOT/third_party" -I"$EVO/media/include" -I"$EVO/pp/include" -I"$EVO/include" "${cflags[@]}")
key=$(printf '%s\0' "$CXX" "${flags[@]}" | sha256sum | cut -d' ' -f1)
cache="$ROOT/test-output/player-host-objects/$key";mkdir -p "$cache"
sources=("$ROOT/$main" "$ROOT/src/"{player,vlc_backend,pcm_out,bitstream,packet_storage,subtitle_renderer,subtitles,video_frame,yuv_convert,util,bidi}.cpp)
case "$mode" in
 network) sources+=("$ROOT/src/netstream.cpp" "$ROOT/src/http.cpp");;
 file) ;;
 *) echo "Unknown player fixture mode" >&2;exit 2;;
esac
objects=()
for source in "${sources[@]}";do
 object="$cache/$(basename "$source").o"
 if python3 "$ROOT/tools/object_needs_rebuild.py" "$object" "$object.d" "$object.flags" "$key";then
  "$CXX" "${flags[@]}" -MMD -MF "$object.d" -c "$source" -o "$object"
  printf '%s\n' "$key" > "$object.flags"
 fi
 objects+=("$object")
done
link=()
case "$wrap" in
 seek) link+=(-Wl,--wrap=avformat_seek_file -Wl,--wrap=av_seek_frame);;
 none) ;;
 *) echo "Unknown fixture wrapper" >&2;exit 2;;
esac
"$CXX" -pthread "${objects[@]}" "${libs[@]}" "${link[@]}" -o "$ROOT/test-output/$output"
