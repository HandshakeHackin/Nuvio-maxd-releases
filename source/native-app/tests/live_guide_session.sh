#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/live-guide-session"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text()
b.joinpath('live_guide_session_fields.hpp').write_text(h[h.index('    void live_guide_reset();'):h.index('    void live_add(')])
b.joinpath('session.cpp').write_text((r/'gl/session_live_guide.cpp').read_text().replace('#include "session.hpp"','#include "live_guide_session_boundary.hpp"'))
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -pthread -I"$B" -I"$R/tests" -I"$R/src" -I"$R/third_party" \
 "$B/session.cpp" "$R/tests/live_guide_session_host.cpp" "$R/src/live_tv.cpp" "$R/src/live_guide.cpp" "$R/src/util.cpp" \
 $(pkg-config --cflags --libs libcurl expat zlib) -o "$B/guide-session-host"
python3 "$R/tests/live_guide_test.py" "$R/test-output/live-guide-host" "$B/guide-session-host"
