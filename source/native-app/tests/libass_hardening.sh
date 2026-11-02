#!/usr/bin/env bash
# Real, checksum-pinned libass + ASan regression, independent of the installed SDK.
# Usage: tests/libass_hardening.sh /path/to/libass-0.17.5.tar.xz
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARCHIVE=$(realpath "${1:?Pass the pinned libass source archive}")
B=$(mktemp -d)
trap 'rm -rf "$B"' EXIT
python3 - "$R" "$ARCHIVE" <<'PY'
from pathlib import Path
import hashlib,json,sys
r,archive=map(Path,sys.argv[1:]);m=json.loads((r/'dependencies.json').read_text())['maintenanceLibraries']
assert hashlib.sha256(archive.read_bytes()).hexdigest()==m['sources']['libass']['sha256']
for patch in m['libassPatches']:assert hashlib.sha256((r/patch['path']).read_bytes()).hexdigest()==patch['sha256']
PY
tar xf "$ARCHIVE" --strip-components=1 -C "$B"
cd "$B"
for patch_file in "$R"/third_party/libass/*.patch; do patch --batch --fuzz=0 -p1 < "$patch_file"; done
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}
./configure --prefix="$B/install" --disable-static --enable-shared --disable-fontconfig --disable-require-system-font-provider --disable-asm --disable-libunibreak CFLAGS='-O1 -g0 -fsanitize=address -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address'
make -j"${JOBS:-2}"
mkdir "$B/include"
ln -s "$B/libass" "$B/include/ass"
${CC:-cc} -O1 -g -fsanitize=address -I"$B/include" "$R/tests/subtitle_input_host.c" -L"$B/libass/.libs" -Wl,-rpath,"$B/libass/.libs" -lass -o "$B/input-test"
"$B/input-test"
