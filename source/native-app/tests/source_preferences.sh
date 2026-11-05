#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/source-preferences"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PYGEN'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:])
(b/'session_sources_test.cpp').write_text((r/'gl/session_sources.cpp').read_text().replace('#include "session.hpp"','#include "source_preferences_boundary.hpp"'))
s=(r/'gl/session_detail.cpp').read_text()
start=s.index('void Session::detail_refresh_streams()')
end=s.index('\n}\n\n} // namespace sx',start)+2
(b/'session_refresh_test.cpp').write_text('#include "source_preferences_boundary.hpp"\nnamespace sx { using stream_display::view_of;\n'+s[start:end]+'\n}\n')
PYGEN
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$ROOT/tests" -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/ui" -I"$ROOT/third_party" \
 "$ROOT/tests/source_preferences_host.cpp" "$B/session_sources_test.cpp" "$B/session_refresh_test.cpp" \
 "$ROOT/gl/episode_playback.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/util.cpp" \
 -o "$B/source-preferences-host"
"$B/source-preferences-host" "$B/ui-scenes.json"
