#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output/track-recovery"
python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);source=(root/'gl/session_watch.cpp').read_text()
refresh=source[source.index('void Session::watch_refresh_tracks()'):source.index('void Session::watch_build_menu()')]
start=source.index('    if (playing.menu)\n',source.index('void Session::watch_input('))
end=source.index('\n    if (!w_trailer_',start)
menu='void Session::menu_input(const hui::InputFrame& in) {\nusing hui::Action;using hui::Direction;\n'+source[start:end]+'\n}\n'
(root/'test-output/track-recovery/track_recovery_production.inc').write_text(refresh+menu)
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror \
 -I"$ROOT/gl" -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" -I"$ROOT/test-output/track-recovery" \
 "$ROOT/tests/track_recovery_host.cpp" -o "$ROOT/test-output/track-recovery-host"
"$ROOT/test-output/track-recovery-host"
