#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -Wall -Wextra -Werror -I"$ROOT/ui" "$ROOT/tests/playback_badges_host.cpp" -o "$ROOT/test-output/playback-badges-host"
"$ROOT/test-output/playback-badges-host"
