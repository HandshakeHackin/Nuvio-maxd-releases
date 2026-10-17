#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -pthread -Wall -Wextra -Werror -I"$ROOT/src" -I"$ROOT/third_party" \
 "$ROOT/tests/tasks_host.cpp" "$ROOT/src/tasks.cpp" -o "$ROOT/test-output/tasks-host"
"$ROOT/test-output/tasks-host"
