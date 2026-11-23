#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/unified-tv"
mkdir -p "$B"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-missing-field-initializers -pthread -ffunction-sections -fdata-sections -I"$R/src" -I"$R/third_party" \
 "$R/tests/unified_tv_host.cpp" "$R/src/unified_tv.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/core"
"$B/core"
