#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -I"$ROOT/src" -I"$ROOT/third_party" $(pkg-config --cflags libcurl) "$ROOT/tests/media_server_host.cpp" "$ROOT/src/"{media_server,http,util}.cpp $(pkg-config --libs libcurl) -o "$ROOT/test-output/media-servers-host"
python3 "$ROOT/tests/media_server_test.py" "$ROOT/test-output/media-servers-host"
