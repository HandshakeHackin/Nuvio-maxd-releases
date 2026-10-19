#!/usr/bin/env bash
# Run after providers.sh with the four unmodified provider scripts, in order:
# moflix.js huhuto.js filmpalast.js hdfilme.js. Uses simulated HTTP only.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${QUICKJS_DIR:?Set QUICKJS_DIR as for tests/providers.sh}"
[ "$#" -eq 4 ] || { echo 'Supply the four German provider scripts in documented order' >&2; exit 2; }
BUILD="$ROOT/test-output/provider-objects"
read -r -a libs <<< "$(pkg-config --libs libcurl openssl)"
read -r -a incs <<< "$(pkg-config --cflags libcurl openssl)"
${HOST_CXX:-g++} -std=c++20 -O0 -pthread -DNUVIO_HAVE_PROVIDERS -I"$ROOT/src" -I"$ROOT/third_party" -I"$QUICKJS_DIR" "${incs[@]}" \
 "$ROOT/tests/providers_german_host.cpp" "$BUILD/stremio.cpp.o" "$BUILD/http.cpp.o" "$BUILD/util.cpp.o" \
 "$QUICKJS_DIR/libquickjs.a" "${libs[@]}" -Wl,--wrap=curl_easy_setopt -ldl -lm -o "$ROOT/test-output/providers-german-host"
"$ROOT/test-output/providers-german-host" "$ROOT/third_party/provider-runtime/runtime.js" "$@"
