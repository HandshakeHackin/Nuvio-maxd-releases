#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -pthread -Wall -Wextra -Werror -I"$R/src" -I"$R/gl" -I"$R/ui" -I"$R/third_party" -I"$R/third_party/hui/src" \
 "$R/tests/features20_host.cpp" "$R/src/config_backup.cpp" "$R/src/config_drive.cpp" "$R/src/feature_http.cpp" "$R/src/stalker.cpp" "$R/src/tvdb.cpp" \
 "$R/src/iptv_library.cpp" "$R/src/http.cpp" "$R/src/live_tv.cpp" "$R/src/util.cpp" "$R/gl/episode_playback.cpp" \
 $(pkg-config --cflags --libs libcurl openssl) -o "$R/test-output/features20-host"
TASK_FEATURE_TMP=$(mktemp -d)
trap 'rm -rf "$TASK_FEATURE_TMP"' EXIT
"$R/test-output/features20-host" "$TASK_FEATURE_TMP"
