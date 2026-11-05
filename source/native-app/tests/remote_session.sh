#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/remote-session"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session_remote.cpp').read_text();h=(r/'gl/session.hpp').read_text()
core=(r/'gl/session.cpp').read_text();a=core.index('void Session::open_dropdown(');z=core.index('void Session::dropdown_confirm()',a)
b.joinpath('session.cpp').write_text(s.replace('#include "session.hpp"','#include "remote_session_boundary.hpp"')+'\nnamespace sx {\n'+core[a:z]+'}\n')
b.joinpath('remote_learning_method.hpp').write_text(next(line for line in h.splitlines() if 'bool remote_learning()const' in line)+'\n')
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -I"$B" -I"$R/tests" -I"$R/gl" -I"$R/third_party" "$B/session.cpp" "$R/tests/remote_session_host.cpp" -o "$B/test"
"$B/test" "$B/previews.json"
