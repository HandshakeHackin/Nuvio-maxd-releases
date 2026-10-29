#!/usr/bin/env bash
# Build checksum-pinned maintenance libraries without changing the base SDK.
# Usage: build_maintenance_libraries.sh BASE_SDK WORK_DIRECTORY
# Then export PS5_PAYLOAD_SDK=WORK_DIRECTORY/sdk before gl/build.sh.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BASE=$(realpath "${1:?Pass the pinned base Payload SDK}")
W=$(realpath -m "${2:?Pass a separate build directory}")
SDK=$W/sdk
[[ "$W" != "$BASE" && "$W/" != "$BASE/"* ]] || { echo 'Build directory must be outside the base SDK' >&2; exit 2; }
SDK_CANONICAL=$(realpath -m "$SDK")
[[ "$SDK_CANONICAL/" != "$BASE/"* && "$BASE/" != "$SDK_CANONICAL/"* ]] || {
  echo 'Overlay SDK and base SDK must not overlap or alias each other' >&2; exit 2;
}
mkdir -p "$W/downloads"
python3 - "$ROOT" "$W" "$BASE" <<'PY'
from pathlib import Path
import hashlib,json,sys,urllib.request
root,work,base=map(Path,sys.argv[1:]);pins=json.loads((root/'dependencies.json').read_text())
for name,digest in pins['sdk']['coreFileSha256'].items():
    assert hashlib.sha256((base/name).read_bytes()).hexdigest()==digest, 'Use the unmodified pinned base SDK: '+name
for item in pins['maintenanceLibraries']['sources'].values():
    target=work/'downloads'/item['archive']
    if not target.exists():
        tmp=target.with_suffix(target.suffix+'.part')
        with urllib.request.urlopen(item['url'],timeout=120) as r,tmp.open('wb') as f:
            while block:=r.read(1024*1024):f.write(block)
        tmp.replace(target)
    assert hashlib.sha256(target.read_bytes()).hexdigest()==item['sha256'], 'Source checksum mismatch: '+target.name
PY
if [ ! -d "$SDK" ]; then cp -al "$BASE" "$SDK"; fi
# Hardlink clone: unlink/replace every changed leaf, never truncate base files.
python3 - "$SDK" "$BASE" <<'PY'
from pathlib import Path
import sys
sdk,base=map(Path,sys.argv[1:])
for p in (sdk/'target/user/homebrew/lib').rglob('*.la'):
    s=p.read_text();n=s.replace('/opt/ps5-payload-sdk',str(sdk)).replace(str(base),str(sdk))
    if n!=s:p.unlink();p.write_text(n)
PY
source "$SDK/toolchain/prospero.sh"
export CFLAGS='-O2 -g0' CXXFLAGS='-O2 -g0'
export SOURCE_DATE_EPOCH=1793232000
export PATH="$SDK/bin:$PATH"
install_overlay() {
  python3 - "$1" "$SDK/target" <<'PY'
from pathlib import Path
import shutil,sys
src,dst=map(Path,sys.argv[1:])
for p in src.rglob('*'):
    q=dst/p.relative_to(src)
    if p.is_dir():q.mkdir(parents=True,exist_ok=True)
    else:
        if q.is_symlink() or q.exists():q.unlink()
        if p.is_symlink():q.symlink_to(p.readlink())
        else:shutil.copy2(p,q)
PY
  "$PS5_CROSS_FIX_ROOT" "$SDK/target/user/homebrew"
}
cd "$W"
if [ ! -f openssl.done ]; then
  tar xf downloads/openssl-3.5.9.tar.gz
  cd openssl-3.5.9
  CC=prospero-clang ./Configure BSD-x86_64 no-tests no-apps no-shared --prefix=/user/homebrew -O2 -g0
  make -j"${JOBS:-2}" build_sw
  make install_sw DESTDIR="$W/stage-openssl"
  install_overlay "$W/stage-openssl"
  cd "$W"; touch openssl.done
fi
if [ ! -f libass.done ]; then
  tar xf downloads/libass-0.17.5.tar.xz
  cd libass-0.17.5
  ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd --enable-static --disable-shared --disable-asm --enable-large-tiles
  make -j"${JOBS:-2}"
  make install DESTDIR="$W/stage-libass"
  install_overlay "$W/stage-libass"
  cd "$W"; touch libass.done
fi
if [ ! -f curl.done ]; then
  tar xf downloads/curl-8.23.0.tar.xz
  cd curl-8.23.0
  ./configure --prefix=/user/homebrew --host=x86_64-pc-freebsd --enable-static --disable-shared --with-openssl --with-ca-bundle=/app0/ca-bundle.crt --disable-docs
  make -j"${JOBS:-2}"
  make install DESTDIR="$W/stage-curl"
  install_overlay "$W/stage-curl"
  cd "$W"; touch curl.done
fi
python3 "$ROOT/tools/validate_maintenance_libraries.py" "$SDK"
printf 'Ready: export PS5_PAYLOAD_SDK=%q\n' "$SDK"
