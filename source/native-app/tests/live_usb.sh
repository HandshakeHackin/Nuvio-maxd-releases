#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -Wl,--wrap=dup,--wrap=fdopendir,--wrap=fstatat,--wrap=openat -I"$R/src" -I"$R/third_party" "$R/tests/live_usb_host.cpp" "$R/src/live_tv.cpp" $(pkg-config --cflags --libs libcurl) -o "$R/test-output/live-usb-host"
"$R/test-output/live-usb-host"
