#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/connected_services_host.cpp" \
    "$ROOT/src/connected_services.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" \
    $(pkg-config --cflags --libs openssl libcurl) -o "$ROOT/test-output/connected-services-host"
"$ROOT/test-output/connected-services-host"
