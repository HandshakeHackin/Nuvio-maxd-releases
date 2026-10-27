#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output/multiview-controls"
python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);s=(root/'gl/session_multiview.cpp').read_text()
a=s.index('void Session::multi_input(');b=s.index('{',a)+1;depth=1;e=b
while depth:
 depth+=(s[e]=='{')-(s[e]=='}');e+=1
(root/'test-output/multiview-controls/multiview_input_body.hpp').write_text(s[a:e])
PY
${HOST_CXX:-g++} -std=c++20 -Wall -Wextra -Werror -I"$ROOT/src" -I"$ROOT/third_party/hui/src" -I"$ROOT/test-output/multiview-controls" \
 "$ROOT/tests/multiview_controls_host.cpp" -o "$ROOT/test-output/multiview-controls-host"
"$ROOT/test-output/multiview-controls-host"
