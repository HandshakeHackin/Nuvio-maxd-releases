#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/localization"
mkdir -p "$B"
python3 "$R/tools/build_locales.py" --check
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections -I"$R/src" -I"$R/gl" -I"$R/ui" -I"$R/tests" -I"$R/third_party" -I"$R/third_party/hui/src" -I"$R/third_party/hui/third_party/stb")
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/localization_host.cpp" "$R/ui/dynamic_font.cpp" "$R/third_party/hui/src/gfx/font.cpp" "$R/third_party/hui/src/core/save_file.cpp" -Wl,--gc-sections -o "$B/locale"
"$B/locale" "$R/app/fonts"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);s=(r/'gl/session.cpp').read_text()
a=s.index('void Session::open_dropdown(');z=s.index('void Session::dropdown_confirm()',a);op=s[a:z]
a=s.index('void Session::dropdown_cancel()');z=s.index('// ---------------------------------------------------------------------------',a);cancel=s[a:z]
a=s.index('    if(row==kUiLanguage)');z=s.index('    if(row==SettingsNavigation::search_action)',a);language='void LanguageSession::settings_activate(int row){\n'+s[a:z]+'}\n'
(b/'dialogs.cpp').write_text('#include "localization_cancel.hpp"\nnamespace sx {\n'+(op+cancel).replace('Session::','LocaleCancelSession::')+language+'}\n')
PY
${HOST_CXX:-g++} "${flags[@]}" "$R/tests/localization_cancel.cpp" "$B/dialogs.cpp" "$R/src/util.cpp" -Wl,--gc-sections -o "$B/dialogs"
"$B/dialogs"
