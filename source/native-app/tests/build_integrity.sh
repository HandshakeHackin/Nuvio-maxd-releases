#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/build-integrity"
mkdir -p "$B"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation \
 -I"$R/src" -I"$R/third_party" "$R/tests/build_integrity_host.cpp" "$R/src/build_integrity.cpp" \
 $(pkg-config --cflags --libs openssl) -o "$B/verifier"
python3 "$R/tests/build_integrity_test.py" "$B/verifier"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:])
s=(r/'gl/session_integrity.cpp').read_text().replace('#include "session.hpp"','#include "integrity_session_boundary.hpp"')
(b/'session.cpp').write_text(s)
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation \
 -I"$R/tests" -I"$R/src" -I"$R/ui" -I"$R/third_party" -I"$R/third_party/hui/src" \
 "$R/tests/integrity_session_host.cpp" "$B/session.cpp" -o "$B/session"
"$B/session"
echo 'Build integrity: real Ed25519/SHA-256, credits/branding changes, tampering, bounds, cancellation, private-key exclusion and unsigned development passed'
