#!/usr/bin/env bash
# Minimal host build of the same FFmpeg release used by the PS5 overlay.
# Supplies the codecs/protocols exercised by the Native HLS and EVO fixtures.
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
W=$(realpath "${1:?Pass the maintenance directory containing downloads}")
B=${2:-$R/test-output/ffmpeg-7.0.3-host}
mkdir -p "$B"
B=$(realpath "$B")
python3 - "$R" "$W" <<'PY'
import hashlib,json,sys
from pathlib import Path
r,w=map(Path,sys.argv[1:]);p=json.loads((r/'dependencies.json').read_text())['maintenanceLibraries']['sources']['ffmpeg']
assert hashlib.sha256((w/'downloads'/p['archive']).read_bytes()).hexdigest()==p['sha256']
PY
[ -d "$B/FFmpeg-n7.0.3" ] || tar xf "$W/downloads/FFmpeg-n7.0.3.tar.gz" -C "$B"
cp "$B/FFmpeg-n7.0.3/RELEASE" "$B/FFmpeg-n7.0.3/VERSION"
cd "$B/FFmpeg-n7.0.3"
./configure --prefix="$B/install" --cc="${HOST_CC:-gcc}" --cxx="${HOST_CXX:-g++}" \
 --enable-shared --disable-static --enable-pic --disable-programs --disable-doc --disable-debug \
 --disable-avfilter --disable-avdevice --disable-postproc --disable-everything \
 --enable-zlib --enable-openssl --enable-version3 \
 --enable-decoder=h264,hevc,aac,ac3,eac3,dca,flac,mp3,pcm_s16le,pcm_f32le,subrip,ass \
 --enable-parser=h264,hevc,aac,ac3,mpegaudio \
 --enable-demuxer=mov,matroska,hls,mpegts,aac,ac3,mp3,flac,srt,ass,wav \
 --enable-protocol=file,http,https,tcp,tls,crypto --enable-swscale --enable-swresample
make -j"${JOBS:-2}"
make install
