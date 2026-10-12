#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CACHE=${NUVIO_SDK_CACHE:-$ROOT/../.deps/nuvio-opengl-1.0.1}
mkdir -p "$CACHE"
ARCHIVE="$CACHE/sdk.tar.gz"
EXPECTED=aaa2e8957f55e1fc0b654dcb35a36e585f7e635d63952da40a28be7f64fde741
if [ ! -f "$ARCHIVE" ]; then
    curl -fsSL --retry 2 https://github.com/blackbearreloaded/ps5-opengl/releases/download/v1.0.1/ps5-opengl-sdk-1.0.1.tar.gz -o "$ARCHIVE.tmp"
    mv "$ARCHIVE.tmp" "$ARCHIVE"
fi
printf '%s  %s\n' "$EXPECTED" "$ARCHIVE" | sha256sum -c - >&2
mkdir -p "$CACHE/extracted"
tar -xzf "$ARCHIVE" --strip-components=1 -C "$CACHE/extracted"
printf '%s\n' "$CACHE/extracted/sdk"
