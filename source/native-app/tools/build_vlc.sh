#!/usr/bin/env bash
# Build pinned libVLC against the SAME FFmpeg ABI as Native and EVO.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
VENDOR=$ROOT/third_party/vlc
SDK=${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK}
WORK=${VLC_BUILD_ROOT:-$ROOT/build/vlc}
PREFIX=$WORK/prefix-ps5
SRC=$WORK/src/vlc-3.0.24
BUILD=$WORK/build-vlc-ps5
mkdir -p "$WORK/downloads" "$WORK/src" "$PREFIX/include" "$PREFIX/lib/pkgconfig" "$BUILD"
python3 - "$VENDOR" <<'CHECK'
import hashlib,json,sys
from pathlib import Path
p=Path(sys.argv[1]); m=json.loads((p/'UPSTREAM.json').read_text())
for f,h in m['fileSha256'].items():
 if hashlib.sha256((p/f).read_bytes()).hexdigest()!=h:raise SystemExit('VLC source mismatch: '+f)
CHECK
ARCHIVE=$WORK/downloads/vlc-3.0.24.tar.xz
if [ ! -f "$ARCHIVE" ] && [ -f "$ROOT/../vendor-sources/$(basename "$ARCHIVE")" ]; then cp "$ROOT/../vendor-sources/$(basename "$ARCHIVE")" "$ARCHIVE"; fi
if [ ! -f "$ARCHIVE" ]; then curl --fail --location --retry 3 https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz -o "$ARCHIVE"; fi
echo "e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf  $ARCHIVE" | sha256sum -c -
if [ ! -d "$SRC" ]; then
 tar -xf "$ARCHIVE" -C "$WORK/src"
fi
mkdir -p "$WORK/applied-patches"
for p in "$VENDOR"/patches/*.patch; do
 stamp="$WORK/applied-patches/$(basename "$p").sha256"
 digest=$(sha256sum "$p" | cut -d' ' -f1)
 if [ -f "$stamp" ]; then
  [ "$(cat "$stamp")" = "$digest" ] || { echo "VLC patch changed: use a fresh VLC_BUILD_ROOT" >&2; exit 1; }
 else
  patch -d "$SRC" -p1 -s < "$p"
  echo "$digest" > "$stamp"
 fi
done
# Private dependency view: rewrite libtool/pkg-config paths without touching SDK.
python3 - "$SDK" "$PREFIX" <<'PREFIX_PY'
from pathlib import Path
import sys
sdk=Path(sys.argv[1]).resolve()/'target/user/homebrew'; p=Path(sys.argv[2]).resolve()
for sub in ('include','lib'):
 for f in (sdk/sub).iterdir():
  if f.name=='pkgconfig':continue
  target=p/sub/f.name
  if f.suffix=='.la':
   if target.is_symlink():target.unlink()
   target.write_text(f.read_text().replace('/opt/ps5-payload-sdk/target/user/homebrew',str(p)).replace('/user/homebrew',str(p)))
  elif not target.exists():target.symlink_to(f)
for f in (sdk/'lib/pkgconfig').glob('*.pc'):
 (p/'lib/pkgconfig'/f.name).write_text(f.read_text().replace('/opt/ps5-payload-sdk/target/user/homebrew',str(p)).replace('/user/homebrew',str(p)))
PREFIX_PY
export CC=$SDK/bin/prospero-clang CXX=$SDK/bin/prospero-clang++ AR=$SDK/bin/prospero-ar RANLIB=$SDK/bin/prospero-ranlib NM=$SDK/bin/prospero-nm STRIP=$SDK/bin/prospero-strip
export VLC_PS5_PREFIX=$PREFIX PKG_CONFIG=$VENDOR/pkg-config PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig
printf '' > "$WORK/empty.c"
"$CC" -c "$WORK/empty.c" -o "$WORK/empty.o"
if [ -L "$PREFIX/lib/libm.a" ]; then rm "$PREFIX/lib/libm.a"; fi
"$AR" rcs "$PREFIX/lib/libm.a" "$WORK/empty.o"
# VLC's adaptive HLS demux needs the real MPEG-TS tables module.
DVB_ARCHIVE=$WORK/downloads/libdvbpsi-1.3.3.tar.bz2
if [ ! -f "$DVB_ARCHIVE" ] && [ -f "$ROOT/../vendor-sources/$(basename "$DVB_ARCHIVE")" ]; then cp "$ROOT/../vendor-sources/$(basename "$DVB_ARCHIVE")" "$DVB_ARCHIVE"; fi
if [ ! -f "$DVB_ARCHIVE" ]; then curl --fail --location --retry 3 https://download.videolan.org/pub/libdvbpsi/1.3.3/libdvbpsi-1.3.3.tar.bz2 -o "$DVB_ARCHIVE"; fi
echo "02b5998bcf289cdfbd8757bedd5987e681309b0a25b3ffe6cebae599f7a00112  $DVB_ARCHIVE" | sha256sum -c -
if [ ! -f "$PREFIX/lib/libdvbpsi.a" ]; then
 tar -xf "$DVB_ARCHIVE" -C "$WORK/src"
 (cd "$WORK/src/libdvbpsi-1.3.3"; ./configure --host=x86_64-pc-freebsd --prefix="$PREFIX" --enable-static --disable-shared --with-pic CFLAGS='-O2 -fPIC'; make -j"${JOBS:-4}" -C src; make -C src install; make install-pkgconfigDATA)
fi
export VLC_BUILD_ROOT=$WORK
bash "$ROOT/tools/build_vlc_matroska.sh"
if [ ! -f "$BUILD/config.status" ] || [ ! -f "$WORK/dvbpsi-configured" ] || [ ! -f "$WORK/matroska-configured" ]; then
 args=(); while IFS= read -r flag; do args+=("--disable-$flag"); done < "$VENDOR/disabled.txt"
 (cd "$BUILD"; "$SRC/configure" --host=x86_64-pc-freebsd --build=x86_64-pc-linux-gnu --prefix="$PREFIX" \
 --enable-static --disable-shared --disable-rpath --disable-maintainer-mode \
 --enable-avcodec --enable-avformat --enable-swscale --enable-libxml2 --enable-libass --enable-freetype --enable-fribidi --enable-harfbuzz \
 --disable-dav1d --enable-matroska --disable-gnutls --disable-smb2 --disable-upnp --disable-dvdread --disable-dvdnav --disable-bluray --disable-archive --enable-dvbpsi \
 ac_cv_func_if_nameindex=yes ax_cv_check_cflags___fstack_protector_strong=no ax_cv_check_cxxflags___fstack_protector_strong=no \
 CFLAGS="-O2 -march=znver2 -fPIC -I$PREFIX/include" CXXFLAGS="-O2 -march=znver2 -fPIC -I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" "${args[@]}")
 touch "$WORK/dvbpsi-configured" "$WORK/matroska-configured"
fi
# Build the explicit integration module set; unused frontends/plugins are omitted.
make -C "$BUILD/compat" -j"${JOBS:-4}"
make -C "$BUILD/src" -j"${JOBS:-4}"
mapfile -t modules < <(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' -e 's/^/lib/' -e 's/$/_plugin.la/' "$VENDOR/plugins.txt")
make -C "$BUILD/modules" -j"${JOBS:-4}" "${modules[@]}"
make -C "$BUILD/lib" -j"${JOBS:-4}"
TARGET=ps5 VLC_PS5_WORK="$WORK" APP_MODULES='vlc_entry__nuviohttp vlc_entry__ps5vdec' bash "$VENDOR/gen-static-modules.sh"
