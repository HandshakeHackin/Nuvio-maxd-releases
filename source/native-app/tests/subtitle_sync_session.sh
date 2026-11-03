#!/bin/bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/subtitle-sync-session"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text();s=(r/'gl/session_watch.cpp').read_text()
b.joinpath('subtitle_sync_session_fields.hpp').write_text(h[h.index('    struct WatchSub'):h.index('    void play_stream(')])
a=s.index('void Session::watch_sync_subtitles()');start=s.index('{',a);end=start+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
b.joinpath('session.cpp').write_text('#include "subtitle_sync_session_boundary.hpp"\nnamespace sx {\n'+s[a:end]+'\n}\n')
PY
${HOST_CXX:-g++} -std=c++20 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$B" -I"$R/tests" -I"$R/src" -I"$R/third_party" "$B/session.cpp" "$R/tests/subtitle_sync_session_host.cpp" "$R/src/stremio.cpp" "$R/src/util.cpp" -o "$B/session-host"
"$B/session-host"
