#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/launcher-features"
mkdir -p "$B"
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function -ffunction-sections -fdata-sections -I"$R/src" -I"$R/ui" -I"$R/gl" -I"$R/tests" -I"$R/third_party" -I"$R/third_party/hui/src")
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/launcher_features_host.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/core"
"$B/core"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session_live_tv.cpp').read_text()
prefix=s[:s.index('void Session::live_reset()')].replace('#include "session.hpp"','#include "launcher_session_boundary.hpp"')
parts=[prefix]
for start,end in [('void Session::live_load()', 'void Session::live_reload()'),('void Session::live_groups()', 'void Session::live_search()')]:
    parts.append(s[s.index(start):s.index(end)])
parts.append('}\n')
for file in ['session_live_options.cpp','session_start_page.cpp']:
    parts.append((r/'gl'/file).read_text().replace('#include "session.hpp"','#include "launcher_session_boundary.hpp"'))
(b/'session.cpp').write_text('\n'.join(parts))
PY
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/launcher_session_host.cpp" "$B/session.cpp" "$R/src/live_tv.cpp" "$R/src/util.cpp" $(pkg-config --cflags --libs libcurl) -Wl,--gc-sections -o "$B/session"
TASK_LAUNCHER_TMP=$(mktemp -d)
trap 'rm -rf "$TASK_LAUNCHER_TMP"' EXIT
"$B/session" "$TASK_LAUNCHER_TMP"
