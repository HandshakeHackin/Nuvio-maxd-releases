#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
read -r -a flags <<< "${HOST_STARTUP_CFLAGS:-$(pkg-config --cflags egl gl)}"
# Compile the actual entry point. UI preview tests use a different main and
# cannot catch an unconditional halt in the production startup sequence.
${HOST_CXX:-g++} -std=c++20 -fsyntax-only -Werror=misleading-indentation \
    -DGL_GLEXT_PROTOTYPES=1 -DHAVE_FRIBIDI -DHAVE_WEBP -DPLATFORM_PS5_NATIVE=1 \
    -I"$ROOT/gl" -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party" \
    -I"$HUI/src" -I"$HUI/third_party/stb" "${flags[@]}" "$ROOT/gl/main.cpp"
echo "Production startup: actual entry point passes conditional-indentation compiler check"
