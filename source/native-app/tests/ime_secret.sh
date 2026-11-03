#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -DPLATFORM_PS5_NATIVE -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/ime_secret_host.cpp" "$ROOT/gl/ime.cpp" "$ROOT/src/util.cpp" -pthread -o "$ROOT/test-output/ime-secret-host"
"$ROOT/test-output/ime-secret-host"
