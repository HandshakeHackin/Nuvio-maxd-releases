#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/hdr-fallback"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session_watch.cpp').read_text();h=(r/'gl/session.hpp').read_text()
a=s.index('void Session::fallback_to_sdr()');z=s.index('void Session::upload_video',a)
method=h[h.index('    bool allow_hdr_fallback()'):h.index('    void fallback_to_sdr();')]
b.joinpath('session.hpp').write_text('''#pragma once
struct Session {
 struct { bool native_hdr=true,auto_hdr_fallback=true; } prefs_;
 bool hdr_fallback_session_=false,uploaded=false;int messages=0;
 void upload_video(bool reuse){uploaded=reuse;}
 void say(const char*,int){++messages;}
 void fallback_to_sdr();
'''+method+'};\n')
b.joinpath('session.cpp').write_text('#include "session.hpp"\n#include "i18n.hpp"\n'+s[a:z])
PY
${HOST_CXX:-g++} -std=c++20 -Wall -Wextra -Werror -I"$B" -I"$R/gl" -I"$R/ui" "$B/session.cpp" "$R/tests/hdr_fallback_host.cpp" -o "$B/test"
"$B/test"
