#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/title-actions"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:])
s=(r/'gl/session_title_actions.cpp').read_text().replace('#include "session.hpp"','#include "title_actions_boundary.hpp"')
d=(r/'gl/session_detail.cpp').read_text();a=d.index('void Session::detail_toggle_watched(');z=d.index('\n}\n',a)+3
(b/'session.cpp').write_text(s+'\nnamespace sx {\n'+d[a:z]+'}\n')
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I"$R/tests" -I"$R/src" -I"$R/gl" -I"$R/ui" -I"$R/third_party" -I"$R/third_party/hui/src" "$R/tests/title_actions_host.cpp" "$B/session.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/test"
"$B/test" "$B/previews.json"
