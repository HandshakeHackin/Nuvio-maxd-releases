#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/torrent-streaming"
rm -rf "$B"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PYGEN'
from pathlib import Path
import sys
root = Path(sys.argv[1])
out = Path(sys.argv[2])
src = (root / "gl/session_torrents.cpp").read_text()
out.joinpath("session_torrents_test.cpp").write_text(
    src.replace('#include "session.hpp"', '#include "torrent_session_boundary.hpp"')
)
PYGEN
read -r -a EXTRA_CFLAGS <<< "$(pkg-config --cflags libavformat libavutil openssl)"
read -r -a EXTRA_LIBS <<< "$(pkg-config --libs libavformat libavutil openssl)"
${HOST_CC:-gcc} -c "$ROOT/third_party/qrcodegen.c" -o "$B/qrcodegen.o"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -Wno-deprecated-declarations -pthread -Wl,--wrap=_Z11now_secondsv \
    "${EXTRA_CFLAGS[@]}" -I"$B" -I"$ROOT/tests" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" \
    "$ROOT/tests/torrent_streaming_host.cpp" \
    "$B/session_torrents_test.cpp" \
    "$ROOT/src/torrent/bencode.cpp" \
    "$ROOT/src/torrent/engine.cpp" \
    "$ROOT/src/torrent/torrent_stream.cpp" \
    "$B/qrcodegen.o" \
    "$ROOT/src/util.cpp" \
    "${EXTRA_LIBS[@]}" -o "$ROOT/test-output/torrent-streaming-host"
"$ROOT/test-output/torrent-streaming-host" "$B/bt-test-dir"
