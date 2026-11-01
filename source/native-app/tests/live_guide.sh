#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -pthread -I"$R/src" -I"$R/third_party" \
 "$R/tests/live_guide_host.cpp" "$R/src/live_tv.cpp" "$R/src/live_guide.cpp" "$R/src/util.cpp" \
 $(pkg-config --cflags --libs libcurl expat zlib) -o "$R/test-output/live-guide-host"
python3 "$R/tests/live_guide_test.py" "$R/test-output/live-guide-host"
