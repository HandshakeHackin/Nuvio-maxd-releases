#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output"
cat > "$ROOT/test-output/lifecycle.srt" <<'SUB'
1
00:00:01,000 --> 00:00:11,000
Embedded captions
SUB
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
    -f lavfi -i 'sine=frequency=440:sample_rate=48000' -i "$ROOT/test-output/lifecycle.srt" -t 12 -c:v libx264 -preset ultrafast -bf 0 -g 48 -c:a aac -c:s mov_text -metadata:s:s:0 language=eng -disposition:s:0 default \
    "$ROOT/test-output/lifecycle.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$ROOT/test-output/lifecycle.mp4" -map 0 -c copy -c:a ac3 "$ROOT/test-output/lifecycle-ac3.mp4"
bash "$ROOT/tests/build_player_host.sh" player-lifecycle-host tests/player_lifecycle_host.cpp file none
"$ROOT/test-output/player-lifecycle-host" "$ROOT/test-output/lifecycle.mp4" "$ROOT/test-output/lifecycle-ac3.mp4"
