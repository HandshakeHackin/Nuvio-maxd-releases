#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror \
 -I"$ROOT/ui" -I"$ROOT/third_party/hui/src" "$ROOT/tests/pin_entry_host.cpp" -o "$ROOT/test-output/pin-entry-host"
"$ROOT/test-output/pin-entry-host"
python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=(p/'gl/session_profiles.cpp').read_text()
choose=s[s.index('void Session::profile_choose('):s.index('void Session::profile_pin_cancel(')]
assert 'ask_text(' not in choose and 'pin_open=true' in choose
assert 'api_profile_select(key, id, pin)' in choose
shell=(p/'ui/shell.cpp').read_text()
assert 's_.profile_pin_input(input)' in shell and 'pin_held || s_.pin_open' in shell
assert 'draw_pin_entry(list,env_,s_.pin_entry)' in shell
assert 'ime_.active() || pin_open' in (p/'gl/session.hpp').read_text()
print('Profile PIN wiring: in-app modal holds input; server verification retained; system IME bypassed')
PY
