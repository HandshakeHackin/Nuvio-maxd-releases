#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$R/src" -I"$R/third_party" "$R/tests/playback_recovery_host.cpp" "$R/src/vlc_cancel.cpp" \
 "$R/src/torrent/bencode.cpp" "$R/src/http.cpp" "$R/src/util.cpp" $(pkg-config --cflags --libs openssl libcurl) -o "$R/test-output/playback-recovery-host"
"$R/test-output/playback-recovery-host"
