# 0.5.34.4 targeted playback test — issue #8

This manual candidate builds on 0.5.34.3. It includes a proposed Native audio-track switching correction, recent-source caching, audio shutdown/video resource safeguards and two VLC seek/rebuffer backports. Physical PS5 Torrentio acceptance remains pending. Live TV, anime timings, settings search and trailers remain staged separately. This is not the normal app/store release.

## Installation

Fully close the app. Download `NuvioMaxd-0.5.34.4-native.ffpfsc`, or `PPSA99288.zip` for installations that use the extracted app folder. Use your existing manual installation method; do not overwrite an image that is still mounted. Keep saved data and one installed copy. Check SHA256SUMS, then confirm **0.5.34.4** in App updates/diagnostics and **05.000.038** in package metadata. Title ID remains **PPSA99288**. Earlier test downloads remain available for rollback.

## Audio-track retest

1. Select **Native** as the player and use the same Torrentio/P2P movie or episode with multiple audio tracks that reproduced issue #8. Keep the original audio-output setting for the first comparison.
2. Play beyond the opening minute. Note the timestamp, change the audio track, and check that the video stays at that point, dialogue matches the picture, and the timer continues normally. Repeat in both directions.
3. Pause at a recognizable scene and change the track. The picture and timestamp should stay at that scene, and playback should remain paused until resumed.
4. Seek to another point, then change the track. Check that the requested seek wins and playback does not jump to the beginning or the download/read-ahead position.
5. If you normally use EVO or VLC, report that separately with the backend name. The clock/packet fix in this candidate specifically changes Native; the other players' paths are different.

A brief resynchronization/buffering period can occur. A restart, frozen picture, reset timer or continuing A/V offset is still a failure. Failed resynchronization should now show an error instead of silently continuing with conflicting clocks.

## Source-cache retest

Open a title/episode and let source searches finish. Back out and reopen the same title within five minutes. Eligible cached torrent results should appear promptly while the app shows that sources are refreshing. Stable direct HTTP results have a shorter sixty-second lifetime; query/fragment links and client-resolver payloads are excluded. Fresh responses replace cached source results. **Reload** deliberately starts a fresh search. Cache is bounded, held only for this running app session, and isolated by profile and source/provider options. It does not download/cache the video or guarantee availability of a cached source.

## Report results in issue #8

Please include build version, selected player, audio-output mode, movie/episode, old/new track codec and language, approximate switch timestamp, and whether you were playing or paused. No account login is needed.

If it still fails, reproduce once and collect the log **before restarting the app**. Open **Settings → App & Updates → Playback diagnostics** to find your app data directory. Through FTP or PS5 Web File Manager, download the actual `nuvio/log.txt` file. Typical path: `/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt` (the sandbox suffix can differ). If available, attach `hui/dev/app.log` from the same `download0` directory too. Diagnostics displays paths; it is not a log uploader. If a file is missing or permission is denied, say which path was available rather than sending a whole folder.

Remove stream URLs, passwords, account/API tokens, authorization headers and pairing codes. Keep version, player/audio format, `player: audio switch`, timing, buffer and error lines. Attach files to https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8 . Keep the issue open until results confirm the fix.

## Validation limits

See PLAYBACK_VALIDATION.json and BUILD-VERIFICATION.json for actual completed checks. Host tests use real FFmpeg and custom HTTP AVIO with simulated console output, not live Torrentio peers or a PS5 sound device. No native HDR/4K output improvement or universal playback fix is claimed. A mixed AAC/AC3 custom-AVIO fixture also stalled before playback in both baseline and corrected player; only its local-file codec-switch variant passed. The broader Native custom-AVIO HLS fixture still times out both before and after this change, before reaching audio-switch coverage. The prior broader host VLC/HLS timeout is also not claimed resolved. The focused MP4/Matroska assertions were run separately.
