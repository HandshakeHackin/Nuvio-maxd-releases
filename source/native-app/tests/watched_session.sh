#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/watched-session"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);methods=''
def grab(s,a,z):return s[s.index(a):s.index(z,s.index(a))]
s=(r/'gl/session_profiles.cpp').read_text();methods+=grab(s,'void Session::refresh_watched_views()','void Session::sync_watched_mark')
s=(r/'gl/session.cpp').read_text();a=s.index('void Session::load_progress()');methods+=s[a:s.index('// ---------------------------------------------------------------------------',a)]
s=(r/'gl/session_browse.cpp').read_text();methods+=grab(s,'std::string episode_badge','std::vector<Item> parse_catalog');methods+=grab(s,'Title Session::make_title','void Session::apply_meta')
s=(r/'gl/session_detail.cpp').read_text();methods+=grab(s,'bool Session::watched(','void Session::detail_toggle_watched');methods+=grab(s,'void Session::detail_fill_title()','void Session::detail_apply_meta()')
(b/'session.cpp').write_text('#include "watched_session_boundary.hpp"\nnamespace sx {\n'+methods+'}\n')
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -pthread -I"$R/tests" -I"$R/gl" -I"$R/src" -I"$R/ui" -I"$R/third_party" \
 "$R/tests/watched_session_host.cpp" "$B/session.cpp" "$R/src/util.cpp" -o "$B/test"
"$B/test"
