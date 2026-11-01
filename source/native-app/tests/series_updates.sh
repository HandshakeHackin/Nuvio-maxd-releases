#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -I"$R/gl" -I"$R/src" -I"$R/third_party" \
 "$R/tests/series_updates_host.cpp" -o "$R/test-output/series-updates-host"
"$R/test-output/series-updates-host"
