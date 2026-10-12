#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output"
mkdir -p "$B"
for codec in ac3 eac3; do
 ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'sine=frequency=440:sample_rate=48000' -t 0.2 -c:a "$codec" -f "$codec" "$B/burst.$codec"
done
read -r -a flags <<< "${HOST_BITSTREAM_CFLAGS:-$(pkg-config --cflags libavformat libavcodec libavutil)}"
read -r -a libs <<< "${HOST_BITSTREAM_LIBS:-$(pkg-config --libs libavformat libavcodec libavutil)}"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -pthread -DPLATFORM_PS5=1 -DPLATFORM_PS5_NATIVE=1 -I"$ROOT/src" -I"$ROOT/third_party" "${flags[@]}" \
 "$ROOT/tests/bitstream_host.cpp" "$ROOT/src/bitstream.cpp" "$ROOT/src/pcm_out.cpp" "${libs[@]}" -o "$B/bitstream-host"
"$B/bitstream-host" "$B/burst.ac3" "$B/burst.eac3"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -I"$ROOT/gl" "$ROOT/tests/scanout_driver_host.cpp" -o "$B/scanout-driver-host"
"$B/scanout-driver-host"
