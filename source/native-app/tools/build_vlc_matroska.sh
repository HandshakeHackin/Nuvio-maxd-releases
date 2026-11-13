#!/usr/bin/env bash
# Native VLC Matroska demux dependencies, compiled for the same PS5 SDK.
set -euo pipefail
WORK=${VLC_BUILD_ROOT:?}
PREFIX=$WORK/prefix-ps5
SDK=${PS5_PAYLOAD_SDK:?}
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CMAKE=${CMAKE:-cmake}
UTF=$WORK/downloads/v4.2.1.tar.gz
if [ ! -f "$UTF" ] && [ -f "$ROOT/../vendor-sources/v4.2.1.tar.gz" ]; then cp "$ROOT/../vendor-sources/v4.2.1.tar.gz" "$UTF"; fi
if [ ! -f "$UTF" ]; then curl --fail --location --retry 3 https://github.com/nemtrif/utfcpp/archive/refs/tags/v4.2.1.tar.gz -o "$UTF"; fi
echo "6d6a5493a111884cc085ee31babfe6d9960c8fb08fc80a64852eaeea8323dbc1  $UTF" | sha256sum -c -
if [ ! -f "$PREFIX/lib/cmake/utf8cpp/utf8cppConfig.cmake" ]; then
 tar -xf "$UTF" -C "$WORK/src"
 for path in "$PREFIX/lib/cmake" "$PREFIX/include/utf8cpp"; do [ ! -L "$path" ] || rm "$path"; done
 "$CMAKE" -S "$WORK/src/utfcpp-4.2.1" -B "$WORK/src/build-utfcpp" \
  -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_CXX_COMPILER="$SDK/bin/prospero-clang++" -DCMAKE_AR="$SDK/bin/prospero-ar" -DCMAKE_RANLIB="$SDK/bin/prospero-ranlib" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib -DUTF8_TESTS=OFF -DUTF8_SAMPLES=OFF -DUTF8_INSTALL=ON
 "$CMAKE" --install "$WORK/src/build-utfcpp"
fi
for spec in \
 'libebml-1.4.7 5b08214f929ee54c6187c370f84235fc9d0f2a2258c4d320d68eae6e2bdfd3f7 libebml' \
 'libmatroska-1.7.2 f3e4d406daf7f1399962c43940e4a87de089474d5bcfbf2e6ca516e98bb87cfc libmatroska'; do
 read -r name digest folder <<< "$spec"
 archive=$WORK/downloads/$name.tar.xz
 if [ ! -f "$archive" ] && [ -f "$ROOT/../vendor-sources/$name.tar.xz" ]; then cp "$ROOT/../vendor-sources/$name.tar.xz" "$archive"; fi
 if [ ! -f "$archive" ]; then curl --fail --location --retry 3 "https://dl.matroska.org/downloads/$folder/$name.tar.xz" -o "$archive"; fi
 echo "$digest  $archive" | sha256sum -c -
 if [ -f "$PREFIX/lib/$folder.a" ] && [ ! -L "$PREFIX/lib/$folder.a" ]; then continue; fi
 tar -xf "$archive" -C "$WORK/src"
 # Detach dependency view symlinks before installing private libraries/headers.
 for path in "$PREFIX/lib/$folder.a" "$PREFIX/lib/cmake" "$PREFIX/include/${folder#lib}"; do
  if [ -L "$path" ]; then rm "$path"; fi
 done
 extra=()
 if [ "$folder" = libmatroska ]; then
  # EBML 1.4.7 exports its header-only utf8cpp target; load that dependency
  # before the older Matroska 1.7.2 project imports EBMLTargets.cmake.
  printf 'find_package(utf8cpp CONFIG REQUIRED)\n' > "$WORK/utf8cpp-dependency.cmake"
  extra+=("-DCMAKE_PROJECT_INCLUDE=$WORK/utf8cpp-dependency.cmake")
 fi
 "$CMAKE" -S "$WORK/src/$name" -B "$WORK/src/build-$name" \
  -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_C_COMPILER="$SDK/bin/prospero-clang" -DCMAKE_CXX_COMPILER="$SDK/bin/prospero-clang++" \
  -DCMAKE_AR="$SDK/bin/prospero-ar" -DCMAKE_RANLIB="$SDK/bin/prospero-ranlib" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON -Dutf8cpp_DIR="$PREFIX/lib/cmake/utf8cpp" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON "${extra[@]}"
 "$CMAKE" --build "$WORK/src/build-$name" -j"${JOBS:-4}"
 "$CMAKE" --install "$WORK/src/build-$name"
done
