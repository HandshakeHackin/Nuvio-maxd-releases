#!/usr/bin/env bash
# Sourced by build_maintenance_libraries.sh after the base overlay is ready.
# All downloads are verified by the parent before this code runs.
set -euo pipefail
HOST_CMAKE=${HOST_CMAKE:-cmake}
MEDIA_RECIPE_HASH=$(sha256sum "${BASH_SOURCE[0]}" | cut -d' ' -f1)
HBROOT="$SDK/target/user/homebrew"
cmake_media() {
  "$HOST_CMAKE" -S "$W/$1" -B "$W/build-$1" \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain/prospero.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_INSTALL_PREFIX=/user/homebrew -DBUILD_SHARED_LIBS=OFF "${@:2}"
  "$HOST_CMAKE" --build "$W/build-$1" -j"${JOBS:-2}"
  DESTDIR="$W/stage-$1" "$HOST_CMAKE" --install "$W/build-$1"
  install_overlay "$W/stage-$1"
}
media_extract() {
  local archive=$1 directory=$2
  [ -d "$W/$directory" ] || tar xf "$W/downloads/$archive" -C "$W"
}
media_needed() { [ ! -f "$W/$1.done" ] || [ "$(cat "$W/$1.done")" != "$MEDIA_RECIPE_HASH" ]; }
media_done() { printf '%s\n' "$MEDIA_RECIPE_HASH" > "$W/$1.done"; }
if media_needed expat-2.10.0; then
  media_extract expat-2.10.0.tar.xz expat-2.10.0
  # Expat uses the SDK's arc4random implementation, not Linux syscalls.
  cmake_media expat-2.10.0 -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=OFF \
    -DEXPAT_WITH_GETRANDOM=OFF -DEXPAT_WITH_SYS_GETRANDOM=OFF -DEXPAT_WITH_GETENTROPY=OFF \
    -DEXPAT_WITH_ARC4RANDOM=ON -DEXPAT_WITH_ARC4RANDOM_BUF=OFF
  media_done expat-2.10.0
fi
if media_needed libpng-1.6.59; then
  media_extract libpng-1.6.59.tar.gz libpng-1.6.59
  cmake_media libpng-1.6.59 -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
    -DZLIB_LIBRARY="$HBROOT/lib/libz.a" -DZLIB_INCLUDE_DIR="$HBROOT/include"
  media_done libpng-1.6.59
fi
if media_needed freetype-2.15.0; then
  media_extract freetype-2.15.0.tar.xz freetype-2.15.0
  # libass supplies shaping through HarfBuzz. Avoid the FreeType/HarfBuzz link cycle.
  cmake_media freetype-2.15.0 -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HVF=ON \
    -DFT_REQUIRE_PNG=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_BZIP2=ON \
    -DZLIB_LIBRARY="$HBROOT/lib/libz.a" -DZLIB_INCLUDE_DIR="$HBROOT/include" \
    -DPNG_LIBRARY="$HBROOT/lib/libpng16.a" -DPNG_PNG_INCLUDE_DIR="$HBROOT/include/libpng16" \
    -DBZIP2_LIBRARY_RELEASE="$HBROOT/lib/libbz2.a" -DBZIP2_INCLUDE_DIR="$HBROOT/include"
  media_done freetype-2.15.0
fi
if media_needed harfbuzz-15.0.0; then
  media_extract harfbuzz-15.0.0.tar.xz harfbuzz-15.0.0
  cmake_media harfbuzz-15.0.0 -DHB_HAVE_FREETYPE=ON -DHB_HAVE_GLIB=OFF -DHB_HAVE_ICU=OFF \
    -DHB_BUILD_UTILS=OFF -DHB_BUILD_SUBSET=OFF -DHB_BUILD_RASTER=OFF -DHB_BUILD_VECTOR=OFF \
    -DHB_BUILD_GPU=OFF -DHB_BUILD_GPU_DEMO=OFF -DBUILD_TESTING=OFF \
    -DFREETYPE_LIBRARY_RELEASE="$HBROOT/lib/libfreetype.a" -DFREETYPE_INCLUDE_DIRS="$HBROOT/include/freetype2"
  media_done harfbuzz-15.0.0
fi
if media_needed fribidi-1.0.18; then
  media_extract fribidi-1.0.18.tar.xz fribidi-1.0.18
  (cd "$W/fribidi-1.0.18"; ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd \
    --enable-static --disable-shared; \
    make -j"${JOBS:-2}" -C lib; make -C lib install DESTDIR="$W/stage-fribidi-1.0.18"; \
    make install-pkgconfigDATA DESTDIR="$W/stage-fribidi-1.0.18")
  install_overlay "$W/stage-fribidi-1.0.18"
  media_done fribidi-1.0.18
fi
if media_needed ffmpeg-7.0.3; then
  media_extract FFmpeg-n7.0.3.tar.gz FFmpeg-n7.0.3
  # GitHub tag archives omit VERSION. Do not inherit an enclosing app Git tag.
  cp "$W/FFmpeg-n7.0.3/RELEASE" "$W/FFmpeg-n7.0.3/VERSION"
  (cd "$W/FFmpeg-n7.0.3"; ./configure --prefix=/user/homebrew --enable-cross-compile \
    --arch=x86_64 --target-os=freebsd --enable-static --disable-shared --enable-pic \
    --cc="$CC" --cxx="$CXX" --nm="$NM" --ar="$AR" --ranlib="$RANLIB" --strip="$STRIP" \
    --pkg-config="$PKG_CONFIG" --enable-openssl --enable-version3 --enable-gpl --enable-libx264 \
    --enable-libfreetype --enable-libfribidi --enable-libharfbuzz --enable-libass \
    --disable-programs --disable-doc --disable-debug --extra-cflags='-O2 -g0'; \
    make -j"${JOBS:-2}"; make install DESTDIR="$W/stage-ffmpeg-7.0.3")
  install_overlay "$W/stage-ffmpeg-7.0.3"
  media_done ffmpeg-7.0.3
fi
