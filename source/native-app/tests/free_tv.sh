#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/free-tv"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text();s=(r/'gl/session_live_tv.cpp').read_text()
start=h.index('    void free_tv_toggle();');end=h.index('    std::vector<free_tv::Country> free_tv_countries_;',start)+len('    std::vector<free_tv::Country> free_tv_countries_;')
(b/'free_tv_session_fields.hpp').write_text(h[start:end])
(b/'free_tv_guide_fields.hpp').write_text(h[h.index('    void live_guide_reset();'):h.index('    void live_add(')])
parts=[s[:s.index('void Session::live_reset()')].replace('#include "session.hpp"','#include "free_tv_session_boundary.hpp"')]
for name in ('live_reset','live_load','live_save','live_reload'):
    a=s.index(('bool' if name=='live_save' else 'void')+' Session::'+name+'(');start=s.index('{',a);end=start+1;depth=1
    while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
    parts.append(s[a:end])
parts.append('}\n');parts.append((r/'gl/session_free_tv.cpp').read_text().replace('#include "session.hpp"','#include "free_tv_session_boundary.hpp"'))
parts.append((r/'gl/session_unified_tv.cpp').read_text().replace('#include "session.hpp"','#include "free_tv_session_boundary.hpp"'))
parts.append((r/'gl/session_live_guide.cpp').read_text().replace('#include "session.hpp"','#include "free_tv_session_boundary.hpp"'))
(b/'session.cpp').write_text('\n'.join(parts))
PY
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -Wno-unused-function -Wno-misleading-indentation -Wno-missing-field-initializers -ffunction-sections -fdata-sections -pthread -I"$B" -I"$R/tests" -I"$R/src" -I"$R/ui" -I"$R/gl" -I"$R/third_party" -I"$R/third_party/hui/src")
# Keep production URL validation; substitute only network IO in the fixture.
${HOST_CXX:-g++} "${flags[@]}" $(pkg-config --cflags libcurl) -Dperform=free_tv_real_transport -c "$R/src/feature_http.cpp" -o "$B/feature-http.o"
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/free_tv_core_host.cpp" "$R/tests/free_tv_session_host.cpp" "$B/session.cpp" \
 "$R/src/free_tv.cpp" "$R/src/unified_tv.cpp" "$R/src/live_tv.cpp" "$R/src/live_guide.cpp" "$R/src/stalker.cpp" "$R/src/config_backup.cpp" "$R/src/stremio.cpp" "$R/src/http.cpp" "$R/src/util.cpp" "$B/feature-http.o" \
 $(pkg-config --cflags --libs libcurl openssl expat zlib) -Wl,--gc-sections -o "$B/free-tv-host"
TASK_FREE_TV_TMP=$(mktemp -d)
trap 'rm -rf "$TASK_FREE_TV_TMP"' EXIT
"$B/free-tv-host" "$TASK_FREE_TV_TMP"
