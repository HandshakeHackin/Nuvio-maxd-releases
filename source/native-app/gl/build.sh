#!/usr/bin/env bash
# Builds Nuvio Max'd for the PS5 and packs it as one .ffpfsc image. Run inside WSL
# (the top-level build.sh installs what this needs, then calls it):
#
#   gl/build.sh [TITLE_ID] [NAME]     default PPSA99288 "Nuvio Max'd"
#
# Output: <OUT_DIR>/<TITLE_ID>/ (the folder), <TITLE_ID>.zip and <TITLE_ID>.ffpfsc
# (default dist/).
#
# The link has our heap, thread, socket and memory fixes (native/), the boilerplate's
# converter and signer, and the ps5-opengl SDK: its libPS5OpenGL.a group, the two Agc
# stub libraries, and the splash wrap that the toolkit's hide_splash_screen() answers.
# The title is a "game" (app/sce_sys/param.json) so that it gets the console's whole
# direct memory.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
python3 "$ROOT/tools/validate_display_metadata.py" "$ROOT/app/sce_sys/param.json"
TITLE="${1:-PPSA99288}"
NAME="${2:-}"
[ -n "$NAME" ] || NAME="Nuvio Max'd"
[[ "$TITLE" =~ ^PPSA[0-9]{5}$ ]] || { echo "Invalid native title ID" >&2; exit 2; }
SDK=${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}
HB=$SDK/target/user/homebrew/lib
BP=${BOILERPLATE_DIR:?Set BOILERPLATE_DIR to the native app toolchain}
KIT=${KIT_DIR:-$ROOT/../.deps/ps5-homebrew-ui}
# Use the app's pinned OpenGL release; validate overrides before linking.
GL=${GL_SDK:-$(bash "$ROOT/tools/fetch_opengl_sdk.sh")}
python3 "$ROOT/tools/validate_build_dependencies.py" "$SDK" "$GL"
TOOL=$BP/build/host/ps5-native-tool
HUI=$ROOT/third_party/hui
NATIVE=$ROOT/native
B=${BUILD_DIR:-$ROOT/build}/app
OUT=${OUT_DIR:-$ROOT/dist}
CXX="$SDK/bin/prospero-clang++"
CC="$SDK/bin/prospero-clang"
COMPILER_RT=${COMPILER_RT:-$("$CC" -print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a}

[ -x "$TOOL" ] || (cd "$BP" && bash tools/build-host-tools.sh)

mkdir -p "$B/obj" "$B/stubs"
export LLVM_NM=${LLVM_NM:-$("${LLVM_CONFIG:-llvm-config}" --bindir)/llvm-nm}
FLAGS=(-std=c++20 -O2 -w -DGL_GLEXT_PROTOTYPES=1 -DHAVE_FRIBIDI -DHAVE_WEBP
	-DPLATFORM_PS5=1 -DPLATFORM_PS5_NATIVE=1 -DHAVE_DHT=1 -DHAVE_UPNP=1 -DMINIUPNP_STATICLIB
	-I"$HUI/src" -I"$HUI/third_party/stb" -I"$ROOT/ui" -I"$ROOT/gl" -I"$ROOT/src" -I"$ROOT/third_party"
	-I"$GL/include" -I"$SDK/target/user/homebrew/include")
EVO="$ROOT/third_party/evo"
EVO_INCLUDES=(-I"$EVO/media/include" -I"$EVO/media/src" -I"$EVO/pp/include" -I"$EVO/include")
FLAGS+=("${EVO_INCLUDES[@]}")

echo ">> compiling"
sources=("$ROOT"/ui/*.cpp "$ROOT"/gl/*.cpp "$ROOT/update/core.cpp"
	"$ROOT/src/nuvio_account.cpp" "$ROOT/src/watch_sync.cpp" "$ROOT/src/connected_services.cpp" "$ROOT/src/connected_preparer.cpp" "$ROOT/src/bidi.cpp" "$ROOT/src/stremio.cpp" "$ROOT/src/http.cpp" "$ROOT/src/util.cpp" "$ROOT/src/tasks.cpp"
	"$ROOT/src/packet_storage.cpp" "$ROOT/src/diagnostics.cpp" "$ROOT/src/player.cpp" "$ROOT/src/netstream.cpp" "$ROOT/src/hwdec_ps5.cpp" "$ROOT/src/yuv_convert.cpp" "$ROOT/src/video_frame.cpp" "$ROOT/src/display_output.cpp"
	"$ROOT/src/evo_backend.cpp" "$ROOT/src/evo_runtime.cpp"
	"$ROOT/src/subtitle_renderer.cpp" "$ROOT/src/subtitles.cpp" "$ROOT/src/pcm_out.cpp" "$ROOT/src/bitstream.cpp" "$ROOT"/src/torrent/*.cpp
	"$HUI"/src/gfx/*.cpp "$HUI"/src/audio/{mixer,cues,wav}.cpp "$HUI/src/platform/ps5/audio_out.cpp" "$HUI/src/core/save_file.cpp" "$HUI/src/core/input.cpp" "$HUI/src/ui/glyphs.cpp"
	"$HUI/src/platform/ps5/display_egl.cpp" "$HUI/src/platform/ps5/system.cpp" "$HUI/src/platform/ps5/pad.cpp")
objs=()
compile_pids=()
for f in "${sources[@]}"; do
	o="$B/obj/$(echo "${f#"$ROOT/"}" | tr '/' '_').o"
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find "$ROOT/gl" "$ROOT/ui" "$ROOT/src" "$ROOT/update" "$ROOT/third_party" -type f \( -name "*.h" -o -name "*.hpp" \) -newer "$o" -print -quit)" ]; then
		"$CXX" "${FLAGS[@]}" -c "$f" -o "$o" &
		compile_pids+=("$!")
	fi
	objs+=("$o")
	while [ "$(jobs -r | wc -l)" -ge "${JOBS:-4}" ]; do sleep 0.2; done
done
compile_failed=0
for pid in "${compile_pids[@]}"; do
	if ! wait "$pid"; then compile_failed=1; fi
done
[ "$compile_failed" -eq 0 ] || { echo "compilation failed; refusing to link stale objects" >&2; exit 1; }
python3 "$ROOT/tools/validate_evo_source.py"
for f in "$EVO/media/src/evo_vdec_ffmpeg.c" "$EVO/media/src/evo_vdec_native.c" \
         "$EVO/pp/src/pp_clock.c" "$EVO/pp/src/pp_playback.c"; do
    o="$B/obj/evo_$(basename "$f").o"
    "$CC" -std=gnu11 -O2 -DEVO_APP_MODULE=1 "${EVO_INCLUDES[@]}" \
        -I"$SDK/target/user/homebrew/include" -c "$f" -o "$o"
    objs+=("$o")
done
for o in "${objs[@]}"; do [ -f "$o" ] || { echo "compile failed ($o)"; exit 1; }; done

"$CXX" -std=c++20 -O2 -fno-exceptions -fno-rtti -c "$BP/tooling/native/app_crt.cpp" -o "$B/app_crt.o"
for f in shims console_curl ps5_modules heap posix_fixes kernel_mem hui_runtime_shims; do
	"$CC" -O2 -w -I"$SDK/target/user/homebrew/include" -c "$NATIVE/$f.c" -o "$B/$f.o"
done
"$CC" -shared -nostdlib -fPIC -Wl,-soname,libSceVideodec2.sprx "$NATIVE/stubs/videodec2.c" -o "$B/stubs/libSceVideodec2.so"

"$CC" -O2 -c "$ROOT/third_party/qrcodegen.c" -o "$B/qrcodegen.o"
objs+=("$B/qrcodegen.o")
python3 "$ROOT/tools/build_hdr_driver.py" "$GL" "$SDK" "$B"
objs+=("$B/hdr_driver.o")
echo ">> linking"
read -r -a DEPS <<< "$("$SDK/bin/prospero-pkg-config" --static --libs libcurl libwebp \
	libavformat libavcodec libavutil libswresample libswscale libass |
	tr ' ' '\n' | grep -E '^-[lL]' | grep -v -E '^(-lm|-lc)$' | tr '\n' ' ')"
"$SDK/bin/prospero-lld" -T "$NATIVE/ps5-app.ld" --eh-frame-hdr \
	--version-script "$BP/tooling/native/app-symbols.map" -e _start -o "$B/pie.elf" \
	--wrap=fcntl -L "$SDK/target/lib" -L "$HB" \
	--wrap=pthread_create --wrap=pipe --wrap=mmap --wrap=mprotect \
	--wrap=malloc --wrap=free --wrap=calloc --wrap=realloc --wrap=reallocf \
	--wrap=posix_memalign --wrap=memalign --wrap=aligned_alloc --wrap=valloc --wrap=malloc_usable_size \
	--wrap=sceSystemServiceHideSplashScreen \
	--wrap=sceVideoOutRegisterBuffers2 --wrap=sceVideoOutClose \
	--allow-multiple-definition \
	--defsym=__cxa_thread_atexit_impl=0 --defsym=__syscall=0 \
	-u ps5_agc_gate2_run \
	"$B/app_crt.o" "$B/shims.o" "$B/console_curl.o" "$B/ps5_modules.o" "$B/heap.o" "$B/posix_fixes.o" "$B/kernel_mem.o" "$B/hui_runtime_shims.o" \
	"${objs[@]}" \
	--start-group \
	"$GL/lib/libPS5OpenGL.a" "$HB/libdht.a" "$HB/libminiupnpc.a" "$HB/libfribidi.a" "${DEPS[@]}" \
	"$SDK/target/lib/libc++.a" "$SDK/target/lib/libc++abi.a" "$SDK/target/lib/libunwind.a" "$SDK/target/lib/libc.a" \
	"$COMPILER_RT" \
	--end-group \
	--as-needed "$SDK"/target/lib/*.so "$B"/stubs/*.so "$GL/lib/libSceAgc.so" "$GL/lib/libSceAgcDriver.so"

python3 "$NATIVE/check_imports.py" "$B/pie.elf" "$SDK/target/lib" "$B/stubs" "$GL/lib"

"$TOOL" link --in "$B/pie.elf" --out "$B/eboot.elf" --stub-dir "$SDK/target/lib" \
	--stub "$B/stubs/libSceVideodec2.so" --stub "$GL/lib/libSceAgc.so" --stub "$GL/lib/libSceAgcDriver.so" \
	--module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf >/dev/null

echo ">> packaging $TITLE"
APP=$B/$TITLE
rm -rf "$APP"
mkdir -p "$APP/sce_sys" "$APP/sce_module"
"$TOOL" self --sign --in "$B/eboot.elf" --out "$APP/eboot.bin" --magic 0x1D3D154F >/dev/null
cp "$BP/runtime/libc.prx" "$APP/sce_module/libc.prx"
python3 - "$ROOT/app/sce_sys/param.json" "$APP/sce_sys/param.json" "$TITLE" "$NAME" <<'PY'
import json, sys
p = json.load(open(sys.argv[1]))
title = sys.argv[3]
p["titleId"] = title
p["conceptId"] = title[4:]
p["contentId"] = "UP9000-%s_00-NUVIOPS5NAT00001" % title
p["localizedParameters"]["en-US"]["titleName"] = sys.argv[4]
json.dump(p, open(sys.argv[2], "w"), indent=2)
PY
cp "$ROOT/app/sce_sys/icon0.png" "$APP/sce_sys/"
bash "$BP/tools/validate-assets.sh" "$APP/sce_sys" >/dev/null
mkdir -p "$APP/fonts" "$APP/hui-fonts" "$APP/icons"
cp "$ROOT"/app/fonts/{Inter-Regular,Inter-SemiBold,Montserrat-Medium,NotoSans-Regular,NotoSans-Bold,NotoNaskhArabicUI-Regular,NotoNaskhArabicUI-Bold,NotoSansThai-Regular,NotoSansCJKsc-Regular}.ttf "$APP/fonts/"
# the fonts' license texts travel with them
cp "$ROOT"/app/fonts/*.txt "$APP/fonts/" 2>/dev/null || true
cp "$HUI"/assets/fonts/*.huifont "$HUI"/assets/fonts/*LICENSE*.txt "$APP/hui-fonts/"
cp "$ROOT"/app/assets/icons_4k/*.png "$APP/icons/"
cp -r "$ROOT/app/sounds" "$APP/sounds"
cp "$ROOT/app/ca-bundle.crt" "$APP/"
echo ">> building bundled image update helper"
make -C "$ROOT/update" PS5_PAYLOAD_SDK="$SDK" OUTPUT="$B/nuvio-updater.elf"
cp "$B/nuvio-updater.elf" "$APP/"
cp "$ROOT/../ps5/account-config.json" "$APP/"
# License notices travel with the app: the GPL, the component list, the license text of everything it
# contains (app/licenses/README.txt), and the notices of the toolkit and the OpenGL SDK.
cp -r "$ROOT/app/licenses" "$APP/"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY.md" "$APP/"
mkdir -p "$APP/licenses/evo-player"
cp "$EVO/LICENSE" "$EVO/UPSTREAM.json" "$EVO/INTEGRATION.md" "$APP/licenses/evo-player/"
cp "$ROOT/third_party/qrcodegen.h" "$APP/licenses/"
cp "$BP/LICENSE" "$APP/licenses/native-app-boilerplate-GPL-3.0.txt"
mkdir -p "$APP/licenses/ps5-homebrew-ui" "$APP/licenses/ps5-opengl"
cp "$HUI/LICENSE" "$HUI/THIRD_PARTY_NOTICES.md" "$APP/licenses/ps5-homebrew-ui/"
SDK_DIR="$(dirname "$GL")"
for f in LICENSE LICENSES THIRD_PARTY_NOTICES.md; do [ -e "$SDK_DIR/$f" ] && cp -r "$SDK_DIR/$f" "$APP/licenses/ps5-opengl/"; done
chmod -R 0777 "$APP"
python3 "$ROOT/tools/build_info.py" "$APP/build-info.json" "$TITLE" "$B/hdr_driver.o"
chmod 0777 "$APP/build-info.json"

mkdir -p "$OUT"
if [ -n "${MKPFS:-}" ]; then PACKER=("$MKPFS"); else PACKER=(python3 -m mkpfs); fi
"${PACKER[@]}" -V | grep -Eq "MkPFS[[:space:]]+1\.1\.0" || { echo "MkPFS 1.1.0 is required" >&2; exit 1; }
rm -rf "$OUT/$TITLE" "$OUT/$TITLE.zip" "$OUT/$TITLE.ffpfsc"
cp -Rp "$APP" "$OUT/$TITLE"
# One compressed container wrapping exFAT, matching ShadowMountPlus nested-image loading.
"${PACKER[@]}" pack folder --no-adjust-output-file-extension --version PS5 --verify "$APP" "$OUT/$TITLE.ffpfsc"
if command -v zip >/dev/null; then (cd "$OUT" && zip -qr "$TITLE.zip" "$TITLE"); fi
ls -la "$OUT"
