#!/usr/bin/env bash
set -euo pipefail
O=$1
mkdir -p "$O"
printf '1\n00:00:01,000 --> 00:00:11,000\nEmbedded captions\n' > "$O/captions.srt"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x108:rate=24' \
 -f lavfi -i 'sine=frequency=440:sample_rate=48000' -f lavfi -i 'sine=frequency=880:sample_rate=48000' -i "$O/captions.srt" \
 -t 12 -map 0:v -map 1:a -map 2:a -map 3:s -c:v libx264 -preset ultrafast -bf 0 -g 48 -c:a aac -c:s mov_text \
 -metadata:s:s:0 language=eng -disposition:s:0 default "$O/fixture.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$O/fixture.mp4" -map 0 -c copy -c:s srt "$O/fixture.mkv"
ffmpeg -hide_banner -loglevel error -y -i "$O/fixture.mp4" -map 0:v -map 0:a:0 -c copy -f hls -hls_time 2 -hls_playlist_type vod "$O/fixture.m3u8"
