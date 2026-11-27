#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 "$ROOT/tools/verify_cloudstream_ports.py"
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -Wno-missing-field-initializers \
 -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/cloudstream_host.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/util.cpp" \
 $(pkg-config --cflags --libs libcurl openssl) -o "$ROOT/test-output/cloudstream-host"
"$ROOT/test-output/cloudstream-host" "$ROOT/tests/fixtures/cloudstream-plugins.json"
