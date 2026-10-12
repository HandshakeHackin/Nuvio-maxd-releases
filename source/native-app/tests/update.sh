#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -I"$ROOT/third_party" "$ROOT/tests/update_host.cpp" "$ROOT/update/core.cpp" \
    $(pkg-config --cflags --libs openssl) -Wl,--wrap=renameat -Wl,--wrap=fsync -o "$ROOT/test-output/update-host"
"$ROOT/test-output/update-host"
