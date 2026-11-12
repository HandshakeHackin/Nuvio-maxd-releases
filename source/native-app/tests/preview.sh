#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
B="$ROOT/test-output"
read -r -a bidi_cflags <<< "$(pkg-config --cflags fribidi)"
read -r -a bidi_libs <<< "$(pkg-config --libs fribidi)"
mkdir -p "$B"
if [[ -z "${PREVIEW_MODES:-}" || "${PREVIEW_MODES:-}" =~ title-actions|watched-confirm ]]; then bash "$ROOT/tests/title_actions.sh"; fi
if [[ -z "${PREVIEW_MODES:-}" || "${PREVIEW_MODES:-}" == *remote-* ]]; then bash "$ROOT/tests/remote_session.sh"; fi
if [[ -z "${PREVIEW_MODES:-}" || "${PREVIEW_MODES:-}" =~ torrent-resume|torrent-settings|playback-recovery|diagnostic-summary ]]; then
    bash "$ROOT/tests/torrent_streaming.sh"
fi
if [[ -z "${PREVIEW_MODES:-}" || "${PREVIEW_MODES:-}" == *source-* ]]; then bash "$ROOT/tests/source_preferences.sh"; fi
if [[ -z "${PREVIEW_MODES:-}" || "${PREVIEW_MODES:-}" == *sports-* ]]; then bash "$ROOT/tests/sports_hub.sh"; fi
${HOST_CC:-gcc} -O2 -c "$ROOT/third_party/qrcodegen.c" -o "$B/qr-host.o"
${HOST_CXX:-g++} -std=c++20 -O1 -DGL_GLEXT_PROTOTYPES=1 -DHAVE_FRIBIDI "${bidi_cflags[@]}" \
    -I"$ROOT/ui" -I"$ROOT/src" -I"$ROOT/third_party" -I"$HUI/src" -I"$HUI/third_party/stb" \
    "$ROOT/tests/ui_preview.cpp" "$ROOT/tests/system_host.cpp" "$ROOT/ui/overlays.cpp" "$ROOT/ui/chrome.cpp" \
    "$ROOT/ui/settings.cpp" "$ROOT/ui/page.cpp" "$ROOT/ui/player.cpp" "$ROOT/ui/board.cpp" "$ROOT/ui/library.cpp" "$ROOT/ui/addons.cpp" "$ROOT/ui/grid.cpp" "$ROOT/ui/dynamic_font.cpp" "$ROOT/ui/cards.cpp" "$ROOT/ui/detail.cpp" "$ROOT/ui/person.cpp" "$ROOT/src/bidi.cpp" \
    "$HUI"/src/gfx/*.cpp "$HUI/src/core/save_file.cpp" "$HUI/src/ui/glyphs.cpp" \
    "$B/qr-host.o" "${bidi_libs[@]}" ${HOST_GL_LIBS:--lEGL -lGL} -o "$B/ui-preview"
if [ -z "${PREVIEW_MODES:-}" ]; then
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
    "$B/ui-preview" "$ROOT" "$B/qr-login.png"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
    "$B/ui-preview" "$ROOT" "$B/stream-cards.png" streams

fi

read -r -a modes <<< "${PREVIEW_MODES:-title-actions title-actions-movie watched-confirm de-title-actions ar-title-actions zh-Hans-title-actions ja-title-actions ko-title-actions sports-settings sports-settings-off sports-hub sports-all-nav sports-menu sports-picker sports-teams sports-channels source-preferences source-preferences-last source-ranking torrent-resume torrent-settings playback-recovery diagnostic-summary torrent-buffer remote-remap remote-learn remote-timeout torrents torrents-all live-guide live-grid live-grid-future series-updates library-search library-search-empty library-search-long iptv movie series seasons player tracks chapters chapters-menu home skip skip-focused recap-focused credits-focused next-focused subtitle-preview subtitle-preview-largest subtitle-preview-high-contrast release-notes install-source store-update github-update donation audio-output settings-categories settings-lighting profile-pin cast person language de-cast de-person de-language de-usb-language de-settings-categories de-profile-pin de-login de-release-notes de-movie de-home de-person-last de-cast-last}"
if [ -z "${PREVIEW_MODES:-}" ]; then
    for language in es fr ar zh-Hans ja ko; do
        for scene in language-picker settings-categories login profile-pin person home player usb-language release-notes; do
            modes+=("$language-$scene")
        done
    done
fi
for mode in "${modes[@]}"; do
    EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
        "$B/ui-preview" "$ROOT" "$B/$mode.png" "$mode"
done
