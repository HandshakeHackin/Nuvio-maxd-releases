#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror \
    -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" "$ROOT/tests/lightbar_host.cpp" \
    -o "$ROOT/test-output/lightbar-host"
"$ROOT/test-output/lightbar-host"
