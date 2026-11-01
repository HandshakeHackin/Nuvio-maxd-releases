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
helper=browse[browse.index('std::string episode_badge'):browse.index('std::vector<Item> parse_catalog')]
methods=(r/'gl/session_series_updates.cpp').read_text().replace('#include "session.hpp"','#include "library_artwork_boundary.hpp"').split('namespace sx {',1)[1].rsplit('}',1)[0]+browse[a:z]+browse[browse.index('void Session::reset_library_search()'):browse.rindex('} // namespace sx')]
# Exercise the real Library filter activation branch without unrelated Discover UI.
a=browse.index('    else if (page == Page::library)',browse.index('void Session::filter_activate'))
z=browse.index('// ---------------------------------------------------------------------------\n// Library',a)
methods=helper+browse[browse.index('Title Session::make_title'):browse.index('void Session::apply_meta')]+methods
methods+='void Session::filter_activate(Page page,int filter) {\n'+browse[a:z].replace('else if','if',1)
search=(r/'gl/session_search.cpp').read_text()
methods+=search[search.index('void Session::begin_search()'):search.index('void Session::close_search()')]
assert 'reset_library_search();' in (r/'gl/session_profiles.cpp').read_text()
assert 'reset_library_search();' in (r/'gl/session_tracking.cpp').read_text()
a=core.index('    // Library\n',core.index('void Session::refresh_art'));z=core.index('    // IPTV artwork',a)
art='void Session::refresh_art(const Cursor& cursor) {\n auto fetch=[this](const std::string& url, auto, bool near){if(near && !url.empty())fetched.push_back(url);return url.empty()?0u:1u;};\n enum class ArtKind {poster};\n'+core[a:z]+'}\n'
bg=header[header.index('template <typename T> void Session::bg'):header.rindex('} // namespace sx')]
(b/'session-library.cpp').write_text('#include "library_artwork_boundary.hpp"\nnamespace sx {\n'+methods+art+bg+'}\n')
PY
opts=(-std=c++20 -O1 -Wall -Wextra -Werror -pthread -ffunction-sections -fdata-sections
 -I"$R/tests" -I"$R/src" -I"$R/third_party" -I"$R/third_party/hui/src")
objects=()
for file in "$B/session-library.cpp" "$R/src/stremio.cpp" "$R/src/iptv_library.cpp" "$R/src/util.cpp"; do
 object="$B/$(basename "$file").o"
 "${HOST_CXX:-g++}" "${opts[@]}" -c "$file" -o "$object"
 objects+=("$object")
done
for fixture in library_artwork library_search series_updates_session; do
 "${HOST_CXX:-g++}" "${opts[@]}" -Wl,--gc-sections "$R/tests/${fixture}_host.cpp" "${objects[@]}" -o "$B/$fixture-host"
 "$B/$fixture-host"
done
