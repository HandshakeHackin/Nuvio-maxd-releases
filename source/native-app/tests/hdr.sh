#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -Wl,--wrap=clock_gettime \
    -I"$ROOT/gl" "$ROOT/tests/hdr_output_host.cpp" "$ROOT/gl/hdr_output.cpp" \
    -o "$ROOT/test-output/hdr-output-host"
"$ROOT/test-output/hdr-output-host"
