#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
B="$ROOT/test-output"
mkdir -p "$B"
${HOST_CC:-gcc} -O2 -c "$ROOT/third_party/qrcodegen.c" -o "$B/qr-host.o"
${HOST_CXX:-g++} -std=c++20 -O1 -DGL_GLEXT_PROTOTYPES=1 \
    -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party" -I"$HUI/src" -I"$HUI/third_party/stb" \
    "$ROOT/tests/ui_preview.cpp" "$ROOT/tests/system_host.cpp" "$ROOT/ui/overlays.cpp" "$ROOT/ui/chrome.cpp" \
    "$ROOT/ui/settings.cpp" "$ROOT/ui/page.cpp" "$ROOT/ui/player.cpp" "$ROOT/ui/board.cpp" "$ROOT/ui/dynamic_font.cpp" "$ROOT/ui/cards.cpp" "$ROOT/ui/detail.cpp" "$ROOT/src/bidi.cpp" \
    "$HUI"/src/gfx/*.cpp "$HUI/src/core/save_file.cpp" "$HUI/src/ui/glyphs.cpp" \
    "$B/qr-host.o" ${HOST_GL_LIBS:--lEGL -lGL} -o "$B/ui-preview"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
    "$B/ui-preview" "$ROOT" "$B/qr-login.png"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
    "$B/ui-preview" "$ROOT" "$B/stream-cards.png" streams

for mode in movie series seasons player tracks home skip skip-focused recap-focused credits-focused next-focused subtitle-preview subtitle-preview-high-contrast release-notes install-source store-update github-update donation; do
    EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
        "$B/ui-preview" "$ROOT" "$B/$mode.png" "$mode"
done
