#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CACHE=${NUVIO_SDK_CACHE:-$ROOT/../.deps/nuvio-opengl-1.0.2}
mkdir -p "$CACHE"
ARCHIVE="$CACHE/sdk.tar.gz"
EXPECTED=30d8e51dd062739b629346102be5932728ba714bc50b1bdcdf08306c8843d9a4
if [ ! -f "$ARCHIVE" ]; then
    curl -fsSL --retry 2 https://github.com/LedyBacer/ps5-opengl/releases/download/v1.0.2/ps5-opengl-sdk-1.0.2.tar.gz -o "$ARCHIVE.tmp"
    mv "$ARCHIVE.tmp" "$ARCHIVE"
fi
printf '%s  %s\n' "$EXPECTED" "$ARCHIVE" | sha256sum -c - >&2
mkdir -p "$CACHE/extracted"
tar -xzf "$ARCHIVE" --strip-components=1 -C "$CACHE/extracted"
printf '%s\n' "$CACHE/extracted/sdk"
