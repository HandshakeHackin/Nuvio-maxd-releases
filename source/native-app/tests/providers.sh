#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${QUICKJS_DIR:?Set QUICKJS_DIR to QuickJS 2026-06-04 built with make libquickjs.a}"
BUILD="$ROOT/test-output/provider-objects"
mkdir -p "$BUILD"
CXX=${HOST_CXX:-g++}
read -r -a libraries <<< "$(pkg-config --libs libcurl openssl)"
read -r -a includes <<< "$(pkg-config --cflags libcurl openssl)"
flags=(-std=c++20 -O0 -pthread -DNUVIO_HAVE_PROVIDERS -DPROVIDER_TEST_DIAGNOSTICS
 -I"$ROOT/src" -I"$ROOT/third_party" -I"$QUICKJS_DIR" "${includes[@]}")
key=$(printf '%s\0' "$CXX" "${flags[@]}" | sha256sum | cut -d' ' -f1)
objects=()
for source in "$ROOT/tests/providers_host.cpp" "$ROOT/src/"{providers,stremio,http,util}.cpp; do
 object="$BUILD/$(basename "$source").o"
 if python3 "$ROOT/tools/object_needs_rebuild.py" "$object" "$object.d" "$object.flags" "$key"; then
  "$CXX" "${flags[@]}" -MMD -MF "$object.d" -c "$source" -o "$object"
  printf '%s\n' "$key" > "$object.flags"
 fi
 objects+=("$object")
done
"$CXX" -pthread "${objects[@]}" "$QUICKJS_DIR/libquickjs.a" "${libraries[@]}" -ldl -lm -o "$ROOT/test-output/providers-host"
"$ROOT/test-output/providers-host" "$ROOT/third_party/provider-runtime/runtime.js" "$@"

# Compile the transfer fixture against the exact production request implementation.
"$CXX" "${flags[@]}" "$ROOT/tests/providers_transfer_host.cpp" \
 "$BUILD/stremio.cpp.o" "$BUILD/http.cpp.o" "$BUILD/util.cpp.o" \
 "$QUICKJS_DIR/libquickjs.a" "${libraries[@]}" -Wl,--wrap=curl_easy_setopt -ldl -lm -o "$ROOT/test-output/providers-transfer-host"
"$ROOT/test-output/providers-transfer-host" "$ROOT/third_party/provider-runtime/runtime.js"
"$CXX" -std=c++20 -O1 -I"$ROOT/ui" "$ROOT/tests/dropdown_window_host.cpp" -o "$ROOT/test-output/dropdown-window-host"
"$ROOT/test-output/dropdown-window-host"
