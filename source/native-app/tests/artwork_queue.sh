#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
python3 - "$ROOT/test-output/artwork.png" <<'PY'
import struct,zlib,sys
def chunk(t,data):return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data))
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,3,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+b'\xff'*8)*3))+chunk(b'IEND',b''))
PY
${HOST_CXX:-g++} -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -DGL_GLEXT_PROTOTYPES \
 -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/third_party/hui/src" -I"$ROOT/third_party/hui/third_party/stb" \
 "$ROOT/tests/artwork_queue_host.cpp" "$ROOT/gl/art.cpp" "$ROOT/src/util.cpp" -o "$ROOT/test-output/artwork-queue-host"
"$ROOT/test-output/artwork-queue-host" "$ROOT/test-output/artwork.png"
