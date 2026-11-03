#!/bin/bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$R/src" -I"$R/gl" -I"$R/ui" -I"$R/third_party" "$R/tests/upstream_next_host.cpp" "$R/gl/episode_playback.cpp" "$R/src/stremio.cpp" "$R/src/util.cpp" -o "$R/test-output/upstream-next-host"
"$R/test-output/upstream-next-host"
