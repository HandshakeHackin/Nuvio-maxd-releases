#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/search-navigation"
mkdir -p "$B"
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-function -ffunction-sections -fdata-sections -I"$R/ui" -I"$R/gl" -I"$R/src" -I"$R/tests" -I"$R/third_party" -I"$R/third_party/hui/src")
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/search_navigation_host.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/core"
"$B/core"
python3 - "$R" "$B" <<'PYCODE'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session_search.cpp').read_text();start=s.index('void Session::ask_search(');end=s.index('void Session::close_search()',start)
(b/'session.cpp').write_text('#include "search_session_boundary.hpp"\nnamespace sx {\n'+s[start:end]+'}\n')
PYCODE
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/search_session_host.cpp" "$B/session.cpp" "$R/src/stremio.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/session"
"$B/session"
python3 - "$R" "$B" <<'PYCODE'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'ui/shell.cpp').read_text();start=s.index('bool Shell::route_top_navigation(');end=s.index('\nunsigned Shell::',start)
(b/'navigation.cpp').write_text('#include "audio/cues.hpp"\n#include "navigation_shell_boundary.hpp"\nnamespace sx {using hui::Action;using hui::Direction;\n'+s[start:end]+s[s.index('void Shell::route_search('):s.index('void Shell::route_page(')]+'}\n')
PYCODE
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/navigation_shell_host.cpp" "$B/navigation.cpp" -Wl,--gc-sections -o "$B/navigation"
"$B/navigation"
