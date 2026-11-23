#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/sports-hub"
mkdir -p "$B"
python3 - "$R" "$B" <<'PYGEN'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);h=(r/'gl/session.hpp').read_text()
(b/'sports_session_fields.hpp').write_text(h[h.index('    bool sports_valid('):h.index('    void live_load();')])
source=(r/'gl/session.cpp').read_text()
start=source.index('    set(kSportsEnable,')
end=source.index('\n',source.index('    group("Sports Hub",',start))
helpers='#include "sports_session_boundary.hpp"\nnamespace sx { void Session::sports_settings_snapshot(){\nusing Kind=SettingRow::Kind;\nsettings.rows.assign(8,SettingRow{});\nauto set=[this](int id,Kind kind,const std::string& label,const std::string& value,const std::string& help,bool on=false){auto& row=settings.rows[id];row.kind=kind;row.label=label;row.value=value;row.help=help;row.on=on;};\nauto group=[this](const char* name,std::initializer_list<int> ids){for(int id:ids)settings.rows[id].category=name;};\n'
(b/'session_sports_settings_snapshot.cpp').write_text(helpers+source[start:end]+'\n}}\n')
for name in ['session_sports','session_sports_settings','session_sports_playback']:
 (b/(name+'.cpp')).write_text((r/'gl'/(name+'.cpp')).read_text().replace('#include "session.hpp"','#include "sports_session_boundary.hpp"'))
PYGEN
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread \
 -I"$B" -I"$R/tests" -I"$R/gl" -I"$R/ui" -I"$R/src" -I"$R/third_party" \
 "$R/tests/sports_hub_host.cpp" "$B"/session_sports*.cpp "$R/gl/sports_hub.cpp" "$R/src/util.cpp" \
 -o "$B/sports-hub-host"
"$B/sports-hub-host" "$R/app" "$B"
