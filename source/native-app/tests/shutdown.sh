#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output/shutdown"
# Execute the production shutdown body with lightweight boundary fakes. Pull
# cancellation-member defaults from the real class, including lazy/null flags.
# This does not exercise the PS5 loader, mount API or the full UI object.
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1]); out=root/'test-output/shutdown'
source=(root/'gl/session.cpp').read_text()
start=source.index('void Session::shutdown()')
brace=source.index('{',start); end=brace+1; depth=1
while depth:
    depth += (source[end]=='{')-(source[end]=='}'); end+=1
(out/'shutdown_body.hpp').write_text(source[start:end]+'\n')
header=(root/'gl/session.hpp').read_text()
fields=[]
for name in ['device_cancel_','d_cancel_','d_stream_cancel_','w_episode_cancel_','live_cancel_']:
    match=re.search(r'std::shared_ptr<std::atomic<bool>> '+name+r'\b[^;]*;',header)
    assert match,name
    fields.append(match.group())
(out/'shutdown_fields.hpp').write_text('\n'.join(fields)+'\n')
main=(root/'gl/main.cpp').read_text()
exit_path=main[main.index('if (session.wants_exit())'):]
assert exit_path.index('session.shutdown();') < exit_path.index('session.complete_update_exit();') < exit_path.index('sys::quit();')
PY
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -I"$ROOT/test-output/shutdown" "$ROOT/tests/shutdown_host.cpp" \
    -o "$ROOT/test-output/shutdown-host"
ulimit -c 0
"$ROOT/test-output/shutdown-host"
