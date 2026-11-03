#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -pthread -I"$ROOT/src" -I"$ROOT/third_party" $(pkg-config --cflags libcurl openssl) "$ROOT/tests/plex_host.cpp" "$ROOT/src/"{plex,media_server,http,util}.cpp $(pkg-config --libs libcurl openssl) -o "$ROOT/test-output/plex-host"
"$ROOT/test-output/plex-host"
