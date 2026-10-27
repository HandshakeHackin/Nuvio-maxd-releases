#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/iptv-session"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);header=(root/'gl/session.hpp').read_text()
out.joinpath('iptv_session_fields.hpp').write_text(header[header.index('    void iptv_reset();'):header.index('    void live_load();')])
out.joinpath('session_iptv.cpp').write_text((root/'gl/session_iptv.cpp').read_text().replace('#include "session.hpp"','#include "iptv_session_boundary.hpp"'))
# Isolation checks supplement the production Session tests: normal playback must
# bypass addon subtitles, parallel service connections, episode lookup and history.
watch=(root/'gl/session_watch.cpp').read_text()
assert '? iptv::failure_message(err)' in watch
assert 'if(iptv::library_type(w_item_.type)){iptv_progress(force_finished);return;}' in watch
assert '!iptv::library_type(w_item_.type)) watch_load_addon_subtitles()' in watch
assert '!iptv::library_type(w_item_.type)) watch_load_segments' in watch
assert 'const bool par = !w_trailer_ && w_item_.type != "tv" && !iptv::library_type(w_item_.type)' in watch
PY
${HOST_CXX:-g++} -std=c++20 -O1 -pthread -Wall -Wextra -Werror \
 -I"$B" -I"$ROOT/tests" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" \
 "$B/session_iptv.cpp" "$ROOT/tests/iptv_session_host.cpp" "$ROOT/src/iptv_library.cpp" "$ROOT/src/live_tv.cpp" "$ROOT/src/util.cpp" \
 $(pkg-config --cflags --libs libcurl) -o "$B/iptv-session-host"
python3 "$ROOT/tests/iptv_session_test.py" "$B/iptv-session-host"
