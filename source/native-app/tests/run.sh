#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 "$ROOT/tools/validate_display_metadata.py" "$ROOT/app/sce_sys/param.json"
mkdir -p "$ROOT/test-output"
bash "$ROOT/tests/tracking.sh"
bash "$ROOT/tests/tasks.sh"
bash "$ROOT/tests/media_remote.sh"
if [ -n "${QUICKJS_DIR:-}" ]; then bash "$ROOT/tests/providers.sh"; fi
if [ -n "${PS5_PAYLOAD_SDK:-}" ]; then bash "$ROOT/tests/curl_socket.sh"; fi
bash "$ROOT/tests/startup.sh"
bash "$ROOT/tests/lightbar.sh"
bash "$ROOT/tests/console_lights.sh"
bash "$ROOT/tests/settings_categories.sh"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$ROOT/src" -I"$ROOT/third_party" "$ROOT/tests/account_host.cpp" \
    "$ROOT/src/nuvio_account.cpp" "$ROOT/src/watch_sync.cpp" "$ROOT/src/tracking.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" \
    $(pkg-config --cflags --libs openssl libcurl) -o "$ROOT/test-output/account-host"
python3 "$ROOT/tests/account_test.py"
bash "$ROOT/tests/navigation.sh"
bash "$ROOT/tests/streams.sh"
bash "$ROOT/tests/connected_services.sh"
bash "$ROOT/tests/instant_playback.sh"
bash "$ROOT/tests/episode.sh"
bash "$ROOT/tests/hwdec.sh"
bash "$ROOT/tests/evo.sh"
bash "$ROOT/tests/upstream.sh"
bash "$ROOT/tests/experience.sh"
bash "$ROOT/tests/display.sh"
bash "$ROOT/tests/display_refresh.sh"
bash "$ROOT/tests/presentation_timing.sh"
bash "$ROOT/tests/hdr.sh"
bash "$ROOT/tests/video_safety.sh"
bash "$ROOT/tests/experimental_av.sh"
bash "$ROOT/tests/playback_quality.sh"
bash "$ROOT/tests/audio_pcm.sh"
bash "$ROOT/tests/player_lifecycle.sh"
bash "$ROOT/tests/player_network.sh"
bash "$ROOT/tests/video.sh"
bash "$ROOT/tests/netstream.sh"
bash "$ROOT/tests/update.sh"
