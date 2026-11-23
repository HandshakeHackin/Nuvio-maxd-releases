#!/usr/bin/env bash
# Source this for renderer tests: old distro libass lacks ass_prune_events.
# The native application already links the checksum-pinned 0.17.5 library.
if ! pkg-config --atleast-version=0.17.5 libass; then
    ass_test_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
    ass_test_cache=${NUVIO_HOST_LIBASS_CACHE:-$ass_test_root/test-output/host-libass}
    ass_test_prefix=$ass_test_cache/install
    mkdir -p "$ass_test_cache"
    mapfile -t ass_test_pin < <(python3 - "$ass_test_root/dependencies.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]))['maintenanceLibraries']['sources']['libass']
print(p['archive']);print(p['url']);print(p['sha256'])
PY
    )
    ass_test_archive=$ass_test_cache/${ass_test_pin[0]}
    if [[ ! -f "$ass_test_archive" ]]; then
        if [[ -n ${NUVIO_HOST_LIBASS_ARCHIVE:-} ]]; then
            cp "$NUVIO_HOST_LIBASS_ARCHIVE" "$ass_test_archive"
        else
            curl --fail --location --proto '=https' --tlsv1.2 --max-time 120 "${ass_test_pin[1]}" -o "$ass_test_archive"
        fi
    fi
    python3 - "$ass_test_archive" "${ass_test_pin[2]}" <<'PY'
import hashlib,sys
assert hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest()==sys.argv[2], 'Host libass source checksum mismatch'
PY
    if [[ ! -f "$ass_test_prefix/lib/pkgconfig/libass.pc" ]]; then
        mkdir -p "$ass_test_cache/source" "$ass_test_cache/build"
        tar -xf "$ass_test_archive" -C "$ass_test_cache/source" --strip-components=1
        (
            cd "$ass_test_cache/build"
            "$ass_test_cache/source/configure" --prefix="$ass_test_prefix" --enable-shared --disable-static \
                --disable-asm --disable-fontconfig --disable-require-system-font-provider
            make -j"${JOBS:-3}"
            make install
        )
    fi
    export PKG_CONFIG_PATH="$ass_test_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export LD_LIBRARY_PATH="$ass_test_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export NUVIO_TEST_LIBASS_PREFIX=$ass_test_prefix
    # Keep the restored host FFmpeg choices, but use matching libass headers/lib.
    if [[ -v HOST_QUALITY_CFLAGS ]]; then
        export HOST_QUALITY_CFLAGS="-I$ass_test_prefix/include $HOST_QUALITY_CFLAGS"
    fi
    if [[ -v HOST_QUALITY_LIBS ]]; then
        export HOST_QUALITY_LIBS="-L$ass_test_prefix/lib $HOST_QUALITY_LIBS"
    fi
    pkg-config --atleast-version=0.17.5 libass
fi
# libass.pc advertises include/ass for <ass.h>; this app uses <ass/ass.h>.
# Place its actual parent before any restored older host include directories.
if [[ -n ${NUVIO_TEST_LIBASS_PREFIX:-} && -f "$NUVIO_TEST_LIBASS_PREFIX/lib/pkgconfig/libass.pc" ]]; then
    # A restored PKG_CONFIG_SYSROOT_DIR must not be prepended to this host path.
    export NUVIO_HOST_LIBASS_INCLUDE=$NUVIO_TEST_LIBASS_PREFIX/include
    export NUVIO_HOST_LIBASS_LIBDIR=$NUVIO_TEST_LIBASS_PREFIX/lib
else
    export NUVIO_HOST_LIBASS_INCLUDE=$(pkg-config --variable=includedir libass)
    export NUVIO_HOST_LIBASS_LIBDIR=$(pkg-config --variable=libdir libass)
fi
