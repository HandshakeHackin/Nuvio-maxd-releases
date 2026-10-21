#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BUILD="$ROOT/test-output/trailer-objects"
mkdir -p "$BUILD"
CXX=${HOST_CXX:-g++}
read -r -a includes <<< "$(pkg-config --cflags libcurl)"
read -r -a libs <<< "$(pkg-config --libs libcurl)"
flags=(-std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -I"$ROOT/src" -I"$ROOT/third_party" "${includes[@]}")
key=$(printf '%s\0' "$CXX" "${flags[@]}" | sha256sum | cut -d' ' -f1)
objects=()
for source in "$ROOT/tests/trailers_host.cpp" "$ROOT/src/"{trailers,stremio,http,util}.cpp; do
 object="$BUILD/$(basename "$source").o"
 if python3 "$ROOT/tools/object_needs_rebuild.py" "$object" "$object.d" "$object.flags" "$key"; then
  "$CXX" "${flags[@]}" -MMD -MF "$object.d" -c "$source" -o "$object"
  printf '%s\n' "$key" > "$object.flags"
 fi
 objects+=("$object")
done
"$CXX" -pthread "${objects[@]}" "${libs[@]}" -o "$ROOT/test-output/trailers-host"
"$ROOT/test-output/trailers-host" "$@"
