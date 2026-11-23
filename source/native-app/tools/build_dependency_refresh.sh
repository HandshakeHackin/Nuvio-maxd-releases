#!/usr/bin/env bash
# Build a new, source-pinned candidate overlay. Existing SDK/downloads are read-only.
# Run validate_build_dependencies.py against the sealed output pins before packaging.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
W=$(realpath "${1:?Pass an isolated dependency work directory}")
SDK=$W/sdk
test -d "$SDK/target/user/homebrew"
[[ ! -L "$SDK" && $(realpath "$SDK") == "$W/sdk" ]] || { echo 'Use a private SDK inside the selected work root' >&2;exit 2; }
python3 - "$ROOT" "$W" <<'PY'
from pathlib import Path
import hashlib,json,sys
root,work=map(Path,sys.argv[1:]);pins=json.loads((root/'dependencies.json').read_text())
for item in pins['maintenanceLibraries']['sources'].values():
    archive=work/'downloads'/item['archive']
    assert hashlib.sha256(archive.read_bytes()).hexdigest()==item['sha256'],archive.name
for field in ['libassPatches','libxml2Patches']:
    for patch in pins['maintenanceLibraries'].get(field,[]):
        assert hashlib.sha256((root/patch['path']).read_bytes()).hexdigest()==patch['sha256'],patch['path']
PY
source "$SDK/toolchain/prospero.sh"
export PATH="$SDK/bin:$PATH"
export CFLAGS='-O2 -g0 -fPIC' CXXFLAGS='-O2 -g0 -fPIC'
export SOURCE_DATE_EPOCH=1794528000
HBROOT=$SDK/target/user/homebrew
mkdir -p "$W/receipts"
normalize_paths() {
  python3 - "$HBROOT" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1])
for pattern in ['lib/pkgconfig/*.pc','lib/*.la','bin/*-config']:
    for path in root.glob(pattern):
        if path.is_symlink():continue
        text=path.read_text(errors='strict')
        value=re.sub(r'/(?:opt/ps5-payload-sdk|workspace/[^\s;\'\"]+)/target/user/homebrew','/user/homebrew',text)
        if value!=text:
            path.unlink();path.write_text(value)
PY
  "$PS5_CROSS_FIX_ROOT" "$HBROOT"
}
install_overlay() {
  python3 - "$1" "$SDK/target" <<'PY'
from pathlib import Path
import shutil,sys
source,target=map(Path,sys.argv[1:])
for path in source.rglob('*'):
    destination=target/path.relative_to(source)
    if path.is_dir() and not path.is_symlink():destination.mkdir(parents=True,exist_ok=True)
    else:
        destination.parent.mkdir(parents=True,exist_ok=True)
        if destination.exists() or destination.is_symlink():destination.unlink()
        if path.is_symlink():destination.symlink_to(path.readlink())
        else:shutil.copy2(path,destination)
PY
  normalize_paths
}
component_key() {
  python3 - "$ROOT" "$1" <<'PY'
from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1]);pins=json.loads((root/'dependencies.json').read_text())
source={k.lower():v for k,v in pins['maintenanceLibraries']['sources'].items()}[sys.argv[2]]
record={'source':source,'recipeSha256':hashlib.sha256((root/'tools/build_dependency_refresh.sh').read_bytes()).hexdigest(),'llvmArchiveSha256':pins['hostToolchain']['archiveSha256'],'sourceDateEpoch':1794528000}
print(hashlib.sha256(json.dumps(record,sort_keys=True).encode()).hexdigest())
PY
}
needed() {
  if [[ -n ${REFRESH_COMPONENTS:-} && ,$REFRESH_COMPONENTS, != *,$1,* ]]; then return 1; fi
  [[ ! -f "$W/receipts/$1.done" || $(cat "$W/receipts/$1.done") != "$(component_key "$1")" ]]
}
done_component() { component_key "$1" > "$W/receipts/$1.done"; }
cmake_build() {
  local name=$1 src=$2;shift 2
  cmake -S "$W/$src" -B "$W/build-$name" \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain/prospero.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_INSTALL_PREFIX=/user/homebrew -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF "$@"
  cmake --build "$W/build-$name" -j"${JOBS:-4}"
  DESTDIR="$W/stage-$name" cmake --install "$W/build-$name"
  install_overlay "$W/stage-$name"
  done_component "$name"
}
autotools_build() {
  local name=$1 src=$2;shift 2
  mkdir -p "$W/build-$name"
  (cd "$W/build-$name"; "$W/$src/configure" --prefix=/user/homebrew \
     --host=x86_64-pc-freebsd --build=x86_64-pc-linux-gnu --enable-static --disable-shared "$@";
   make -j"${JOBS:-4}";make install DESTDIR="$W/stage-$name")
  install_overlay "$W/stage-$name"
  done_component "$name"
}
normalize_paths
if needed zlib; then
  cmake_build zlib zlib-1.4 -DZLIB_BUILD_TESTING=OFF -DZLIB_BUILD_SHARED=OFF \
    -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_MINIZIP=OFF -DZLIB_INSTALL=ON
fi
if needed zstd; then
  cmake_build zstd zstd-1.5.7/build/cmake -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON \
    -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_MULTITHREAD_SUPPORT=ON
fi
if needed xz; then
  cmake_build xz xz-5.8.4 -DBUILD_TESTING=OFF -DXZ_TOOL_XZ=OFF -DXZ_TOOL_XZDEC=OFF \
    -DXZ_TOOL_LZMADEC=OFF -DXZ_TOOL_LZMAINFO=OFF -DXZ_DOC=OFF -DXZ_NLS=OFF
fi
if needed libiconv; then
  autotools_build libiconv libiconv-1.19.1 --disable-nls --enable-extra-encodings
fi
if needed libpsl; then
  autotools_build libpsl libpsl-0.23.3 --enable-runtime=no --disable-man --disable-gtk-doc --disable-nls
fi
if needed miniupnpc; then
  cmake_build miniupnpc miniupnp-miniupnpc_2_3_3/miniupnpc \
    -DUPNPC_BUILD_SHARED=OFF -DUPNPC_BUILD_STATIC=ON -DUPNPC_BUILD_TESTS=OFF -DUPNPC_BUILD_SAMPLE=OFF
fi
if needed dht; then
  mkdir -p "$W/build-dht" "$W/stage-dht/user/homebrew/lib" "$W/stage-dht/user/homebrew/include/dht"
  "$CC" $CFLAGS -c "$W/dht-dht-0.28/dht.c" -o "$W/build-dht/dht.o"
  "$AR" rcs "$W/stage-dht/user/homebrew/lib/libdht.a" "$W/build-dht/dht.o"
  cp "$W/dht-dht-0.28/dht.h" "$W/stage-dht/user/homebrew/include/dht/dht.h"
  install_overlay "$W/stage-dht";done_component dht
fi
if needed openssl; then
  (cd "$W/openssl-4.1.0";
   CC="$CC" ./Configure BSD-x86_64 no-tests no-apps no-shared no-module --prefix=/user/homebrew -O2 -g0 -fPIC;
   make -j"${JOBS:-4}" build_sw;make install_sw DESTDIR="$W/stage-openssl")
  install_overlay "$W/stage-openssl";done_component openssl
fi
if needed expat; then
  cmake_build expat expat-2.10.0 -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=OFF \
    -DEXPAT_WITH_GETRANDOM=OFF -DEXPAT_WITH_SYS_GETRANDOM=OFF -DEXPAT_WITH_GETENTROPY=OFF \
    -DEXPAT_WITH_ARC4RANDOM=ON -DEXPAT_WITH_ARC4RANDOM_BUF=OFF
fi
if needed libpng; then
  cmake_build libpng libpng-1.6.59 -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
    -DZLIB_LIBRARY="$HBROOT/lib/libz.a" -DZLIB_INCLUDE_DIR="$HBROOT/include"
fi
if needed freetype; then
  cmake_build freetype freetype-2.15.0 -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HVF=ON \
    -DFT_REQUIRE_PNG=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_BZIP2=ON \
    -DZLIB_LIBRARY="$HBROOT/lib/libz.a" -DZLIB_INCLUDE_DIR="$HBROOT/include" \
    -DPNG_LIBRARY="$HBROOT/lib/libpng16.a" -DPNG_PNG_INCLUDE_DIR="$HBROOT/include/libpng16" \
    -DBZIP2_LIBRARY_RELEASE="$HBROOT/lib/libbz2.a" -DBZIP2_INCLUDE_DIR="$HBROOT/include"
fi
if needed harfbuzz; then
  cmake_build harfbuzz harfbuzz-15.1.0 -DHB_HAVE_FREETYPE=ON -DHB_HAVE_GLIB=OFF -DHB_HAVE_ICU=OFF \
    -DHB_BUILD_UTILS=OFF -DHB_BUILD_SUBSET=OFF -DHB_BUILD_RASTER=OFF -DHB_BUILD_VECTOR=OFF \
    -DHB_BUILD_GPU=OFF -DHB_BUILD_GPU_DEMO=OFF -DBUILD_TESTING=OFF \
    -DFREETYPE_LIBRARY_RELEASE="$HBROOT/lib/libfreetype.a" -DFREETYPE_INCLUDE_DIRS="$HBROOT/include/freetype2"
fi
if needed fribidi; then
  (cd "$W/fribidi-1.0.18"; ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd \
    --enable-static --disable-shared; make -j"${JOBS:-4}" -C lib;
    make -C lib install DESTDIR="$W/stage-fribidi";make install-pkgconfigDATA DESTDIR="$W/stage-fribidi")
  install_overlay "$W/stage-fribidi";done_component fribidi
fi
if needed fontconfig; then
  "$SDK/bin/prospero-meson" setup "$W/build-fontconfig" "$W/fontconfig-2.18.3" \
    --prefix=/user/homebrew --libdir=lib --buildtype=release -Ddefault_library=static \
    -Ddoc=disabled -Dnls=disabled -Dtests=disabled -Dtests-external-fonts=disabled \
    -Dtools=disabled -Dcache-build=disabled -Dxml-backend=expat \
    -Ddefault-fonts-dirs=/app0/fonts -Dadditional-fonts-dirs=[]
  meson compile -C "$W/build-fontconfig" -j"${JOBS:-4}"
  DESTDIR="$W/stage-fontconfig" meson install -C "$W/build-fontconfig"
  install_overlay "$W/stage-fontconfig";done_component fontconfig
fi
if needed libass; then
  if [[ ! -f "$W/libass-0.17.5/.nuvio-hardening-applied" ]]; then
    for patch_file in "$ROOT"/third_party/libass/*.patch;do
      patch --batch --fuzz=0 -p1 -d "$W/libass-0.17.5" < "$patch_file"
    done
    touch "$W/libass-0.17.5/.nuvio-hardening-applied"
  fi
  autotools_build libass libass-0.17.5 --disable-asm --enable-large-tiles
fi
if needed ffmpeg; then
  cp "$W/FFmpeg-n9.0.2/RELEASE" "$W/FFmpeg-n9.0.2/VERSION"
  mkdir -p "$W/build-ffmpeg"
  (cd "$W/build-ffmpeg"; "$W/FFmpeg-n9.0.2/configure" --prefix=/user/homebrew --enable-cross-compile \
    --arch=x86_64 --target-os=freebsd --enable-static --disable-shared --enable-pic \
    --cc="$CC" --cxx="$CXX" --nm="$NM" --ar="$AR" --ranlib="$RANLIB" --strip="$STRIP" \
    --pkg-config="$PKG_CONFIG" --enable-openssl --enable-version3 --enable-gpl --enable-libx264 \
    --enable-libfreetype --enable-libfribidi --enable-libharfbuzz --enable-libass \
    --disable-programs --disable-doc --disable-debug --extra-cflags='-O2 -g0';
    python3 "$ROOT/tools/normalize_ffmpeg_configuration.py" config.h "$SDK";
    make -j"${JOBS:-4}";make install DESTDIR="$W/stage-ffmpeg")
  # Prevent obsolete ABI headers from remaining visible in the new SDK.
  for directory in libavcodec libavdevice libavfilter libavformat libavutil libpostproc libswresample libswscale;do
    [[ ! -d "$HBROOT/include/$directory" ]] || rm -rf "$HBROOT/include/$directory"
  done
  install_overlay "$W/stage-ffmpeg";done_component ffmpeg
fi
if needed libxml2; then
  if [[ ! -f "$W/libxml2-2.15.4/.nuvio-random-patch" ]]; then
    patch --batch --fuzz=0 -p1 -d "$W/libxml2-2.15.4" < "$ROOT/third_party/libxml2/0001-ps5-random.patch"
    touch "$W/libxml2-2.15.4/.nuvio-random-patch"
  fi
  cmake_build libxml2 libxml2-2.15.4 -DCMAKE_C_FLAGS=-DNUVIO_PS5_RANDOM=1 -DHAVE_DECL_GETENTROPY=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_LZMA=ON -DLIBXML2_WITH_ZLIB=ON -DLIBXML2_WITH_ICONV=ON
fi
if needed libwebp; then
  cmake_build libwebp libwebp-1.6.0 -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF \
    -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
    -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
fi
if needed curl; then
  autotools_build curl curl-8.23.0 --with-openssl --with-ca-bundle=/app0/ca-bundle.crt --disable-docs
fi
if [[ -z ${REFRESH_COMPONENTS:-} ]]; then
  python3 "$ROOT/tools/validate_build_dependencies.py" "$SDK" "${GL_SDK:?Set the pinned OpenGL SDK}"
fi
