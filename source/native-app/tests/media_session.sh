#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/media-session"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text()
(b/'media_session_fields.hpp').write_text(h[h.index('    void media_reset();'):h.index('    void iptv_reset();')])
(b/'session_media_servers.cpp').write_text((r/'gl/session_media_servers.cpp').read_text().replace('#include "session.hpp"','#include "media_session_boundary.hpp"'))
PY
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -I"$B" -I"$ROOT/tests" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" "$B/session_media_servers.cpp" "$ROOT/tests/media_session_host.cpp" "$ROOT/src/"{media_server,iptv_library,live_tv,http,util}.cpp $(pkg-config --cflags --libs libcurl) -o "$B/media-session-host"
# Same authenticated fixture API, with Session-specific assertions in the binary.
python3 "$ROOT/tests/media_server_test.py" "$B/media-session-host"
