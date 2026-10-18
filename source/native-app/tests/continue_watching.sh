#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -I"$R/gl" -I"$R/ui" -I"$R/src" -I"$R/third_party" \
 "$R/tests/continue_watching_host.cpp" "$R/gl/continue_watching.cpp" "$R/src/util.cpp" -pthread -o "$R/test-output/continue-watching-host"
"$R/test-output/continue-watching-host"
