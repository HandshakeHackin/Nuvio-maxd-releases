#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$R/test-output/library-artwork"
mkdir -p "$B"
python3 - "$R" "$B" <<'PY'
from pathlib import Path
import sys
r,b=map(Path,sys.argv[1:]);browse=(r/'gl/session_browse.cpp').read_text();core=(r/'gl/session.cpp').read_text();header=(r/'gl/session.hpp').read_text()
a=browse.index('void Session::apply_meta');z=browse.index('// Continue watching',a)
methods=browse[a:z]+browse[browse.index('void Session::refresh_library()'):browse.rindex('} // namespace sx')]
a=core.index('    // Library\n',core.index('void Session::refresh_art'));z=core.index('    // IPTV artwork',a)
art='void Session::refresh_art(const Cursor& cursor) {\n auto fetch=[this](const std::string& url, auto, bool near){if(near && !url.empty())fetched.push_back(url);return url.empty()?0u:1u;};\n enum class ArtKind {poster};\n'+core[a:z]+'}\n'
bg=header[header.index('template <typename T> void Session::bg'):header.rindex('} // namespace sx')]
(b/'session-library.cpp').write_text('#include "library_artwork_boundary.hpp"\nnamespace sx {\n'+methods+art+bg+'}\n')
PY
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -I"$R/tests" -I"$R/src" -I"$R/third_party" -I"$R/third_party/hui/src" \
 "$B/session-library.cpp" "$R/tests/library_artwork_host.cpp" "$R/src/stremio.cpp" "$R/src/util.cpp" -o "$B/library-artwork-host"
"$B/library-artwork-host"
