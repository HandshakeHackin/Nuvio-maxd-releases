#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/media-session"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text()
(b/'media_session_fields.hpp').write_text(h[h.index('    void media_plex_start('):h.index('    void iptv_reset();')])
(b/'session_plex.cpp').write_text((r/'gl/session_plex.cpp').read_text().replace('#include "session.hpp"','#include "media_session_boundary.hpp"'))
(b/'session_media_servers.cpp').write_text((r/'gl/session_media_servers.cpp').read_text().replace('#include "session.hpp"','#include "media_session_boundary.hpp"'))
PY
cc -O2 -c "$ROOT/third_party/qrcodegen.c" -o "$B/qrcodegen.o"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -Wl,--wrap=lstat -I"$B" -I"$ROOT/tests" -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" "$B/session_media_servers.cpp" "$B/session_plex.cpp" "$B/qrcodegen.o" "$ROOT/tests/media_session_host.cpp" "$ROOT/src/"{media_server,plex,iptv_library,live_tv,http,util}.cpp $(pkg-config --cflags --libs libcurl openssl) -o "$B/media-session-host"
# Same authenticated fixture API, with Session-specific assertions in the binary.
python3 "$ROOT/tests/media_server_test.py" "$B/media-session-host"
