#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation \
 -I"$ROOT/gl" -I"$ROOT/third_party/hui/src" "$ROOT/tests/media_remote_host.cpp" "$ROOT/gl/media_remote.cpp" -o "$ROOT/test-output/media-remote-host"
"$ROOT/test-output/media-remote-host"
