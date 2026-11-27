#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/addon-sources"
mkdir -p "$B"
python3 - "$ROOT" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session.cpp').read_text();parts=[]
for name in ('addon_activate','refresh_addons'):
    start=s.index('void Session::'+name+'(');end=s.index('\nvoid Session::',start+1)
    parts.append(s[start:end])
(b/'production.cpp').write_text('#include "addon_sources_boundary.hpp"\nnamespace sx {\n'+'\n'.join(parts)+'\n}\n')
PY
${HOST_CXX:-g++} -std=c++20 -O0 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$ROOT/tests" -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party" \
 "$ROOT/tests/addon_sources_host.cpp" "$B/production.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/util.cpp" -o "$B/host"
"$B/host" "$ROOT/tests/fixtures/cloudstream-plugins.json" "$B/scenes.json"
