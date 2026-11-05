#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/torrent-player"
mkdir -p "$B"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' -f lavfi -i 'sine=frequency=440:sample_rate=48000' -t 10 -c:v libx264 -preset ultrafast -g 24 -c:a aac "$B/tail.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$B/tail.mp4" -c copy -movflags +faststart "$B/front.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$B/tail.mp4" -c copy "$B/video.mkv"
ffmpeg -hide_banner -loglevel error -y -i "$B/tail.mp4" -c copy -f mpegts "$B/video.ts"
ffmpeg -hide_banner -loglevel error -y -i "$B/tail.mp4" -c copy -mpegts_m2ts_mode 1 -f mpegts "$B/video.m2ts"
ffmpeg -hide_banner -loglevel error -y -i "$B/tail.mp4" -f lavfi -i 'sine=frequency=880:sample_rate=48000' -t 10 -map 0:v -map 0:a -map 1:a -c:v copy -c:a aac "$B/dual.mkv"
bash "$ROOT/tests/build_player_host.sh" torrent-player-host tests/torrent_player_host.cpp torrent seek
"$ROOT/test-output/torrent-player-host" "$B/video.mkv" "$B/video.ts" "$B/video.m2ts" "$B/front.mp4" "$B/tail.mp4" "$B/dual.mkv"
