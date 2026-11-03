#!/usr/bin/env bash
# Build the pinned XML/font/image sources for the host and exercise real APIs.
# Source archives come from the same verified download directory as the target overlay.
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
W=$(realpath "${1:?Pass the maintenance build directory}")
B=${MAINTENANCE_HOST_BUILD:-$R/test-output/maintenance-host}
P=$B/install
CMAKE=${HOST_CMAKE:-cmake}
CTEST=${HOST_CTEST:-ctest}
mkdir -p "$B"
python3 - "$R" "$W" <<'PY'
import json,hashlib,sys
from pathlib import Path
r,w=map(Path,sys.argv[1:]);pins=json.loads((r/'dependencies.json').read_text())['maintenanceLibraries']['sources']
for name in ['expat','libpng','freetype','harfbuzz','fribidi']:
 p=pins[name];assert hashlib.sha256((w/'downloads'/p['archive']).read_bytes()).hexdigest()==p['sha256'],name
PY
build() {
 local name=$1;shift
 "$CMAKE" -S "$W/$name" -B "$B/$name" -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH="$P" -DBUILD_SHARED_LIBS=OFF "$@"
 "$CMAKE" --build "$B/$name" -j"${JOBS:-2}"
 "$CMAKE" --install "$B/$name"
}
build expat-2.10.0 -DEXPAT_BUILD_TESTS=ON -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=OFF
"$CTEST" --test-dir "$B/expat-2.10.0" --output-on-failure
build libpng-1.6.59 -DPNG_SHARED=ON -DPNG_STATIC=ON -DPNG_TESTS=ON -DPNG_TOOLS=OFF
"$CTEST" --test-dir "$B/libpng-1.6.59" --output-on-failure
build freetype-2.15.0 -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HVF=ON -DFT_REQUIRE_PNG=ON
build harfbuzz-15.0.0 -DHB_HAVE_FREETYPE=ON -DHB_BUILD_UTILS=OFF -DHB_BUILD_SUBSET=OFF -DHB_BUILD_RASTER=OFF -DHB_BUILD_VECTOR=OFF -DHB_BUILD_GPU=OFF -DHB_BUILD_GPU_DEMO=OFF -DBUILD_TESTING=OFF
# Independent out-of-tree FriBidi build, leaving the PS5 configure state intact.
mkdir -p "$B/fribidi-1.0.18" "$B/fribidi-src"
tar xf "$W/downloads/fribidi-1.0.18.tar.xz" --strip-components=1 -C "$B/fribidi-src"
(cd "$B/fribidi-1.0.18"; "$B/fribidi-src/configure" --prefix="$P" --enable-static --disable-shared; make -C lib -j"${JOBS:-2}"; make -C lib install; make install-pkgconfigDATA)
export LD_LIBRARY_PATH="$P/lib:$P/lib64:${LD_LIBRARY_PATH:-}"
export PKG_CONFIG_PATH="$P/lib/pkgconfig:$P/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror "$R/tests/maintenance_runtime_host.cpp" \
  $(pkg-config --cflags --libs expat libpng freetype2 harfbuzz fribidi) -o "$B/maintenance-runtime-host"
"$B/maintenance-runtime-host" "$R/app/fonts"
# Run the production XMLTV parser against the updated Expat, including its malformed/hostile-input tests.
bash "$R/tests/live_guide.sh"
