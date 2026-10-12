#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -DPLATFORM_PS5=1 \
    -I"$ROOT/src" -I"$ROOT/third_party" \
    "$ROOT/tests/display_output_host.cpp" "$ROOT/src/display_output.cpp" \
    -o "$ROOT/test-output/display-host"
"$ROOT/test-output/display-host"
