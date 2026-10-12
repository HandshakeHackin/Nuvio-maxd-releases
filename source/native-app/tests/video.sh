#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HUI="$ROOT/third_party/hui"
mkdir -p "$ROOT/test-output"
read -r -a ffmpeg_cflags <<< "${HOST_FFMPEG_CFLAGS:-$(pkg-config --cflags libavcodec libavutil)}"
read -r -a ffmpeg_libs <<< "${HOST_FFMPEG_LIBS:-$(pkg-config --libs libavcodec libavutil)}"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
    -frames:v 12 -c:v libx264 -profile:v high -preset medium -bf 2 -g 12 \
    -pix_fmt yuv420p -color_primaries bt709 -color_trc bt709 -colorspace bt709 \
    -color_range tv -f h264 "$ROOT/test-output/h264-regression.h264"
${HOST_CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -Werror -DGL_GLEXT_PROTOTYPES=1 \
    "${ffmpeg_cflags[@]}" \
    -I"$ROOT/src" -I"$ROOT/gl" -I"$HUI/src" -I"$HUI/third_party/stb" \
    "$ROOT/tests/video_host.cpp" "$ROOT/tests/system_host.cpp" \
    "$ROOT/src/video_frame.cpp" "$ROOT/src/yuv_convert.cpp" "$ROOT/gl/video_surface.cpp" "$ROOT/gl/hdr_compositor.cpp" \
    "$HUI"/src/gfx/*.cpp "$HUI/src/core/save_file.cpp" "$HUI/src/ui/glyphs.cpp" \
    ${HOST_GL_LIBS:--lEGL -lGL} "${ffmpeg_libs[@]}" -o "$ROOT/test-output/video-host"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 MESA_SHADER_CACHE_DISABLE=true \
    "$ROOT/test-output/video-host" "$ROOT/test-output/h264-regression.h264"
