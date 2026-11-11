#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/people"
mkdir -p "$B"
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-missing-field-initializers -ffunction-sections -fdata-sections -I"$R/src" -I"$R/gl" -I"$R/ui" -I"$R/third_party" -I"$R/third_party/hui/src" -I"$R/third_party/hui/third_party/stb")
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/people_host.cpp" "$R/src/people.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/metadata"
"$B/metadata"
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/people_ui_host.cpp" "$R/ui/person.cpp" "$R/ui/detail.cpp" "$R/third_party/hui/src/gfx/font.cpp" -Wl,--gc-sections -o "$B/ui"
"$B/ui"
python3 - "$R" "$B" <<'PY2'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session_people.cpp').read_text().replace('#include "session.hpp"','#include "people_session_boundary.hpp"')
h=(r/'gl/session.hpp').read_text();bg=h[h.index('template <typename T> void Session::bg'):h.rindex('} // namespace sx')]
(b/'session.cpp').write_text(s+'\nnamespace sx {\n'+bg+'}\n')
PY2
${HOST_CXX:-g++} "${flags[@]}" -DNUVIO_HAVE_PROVIDERS -I"$R/tests" "$R/tests/people_session_host.cpp" "$B/session.cpp" "$R/src/people.cpp" "$R/src/stremio.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/session"
"$B/session"
