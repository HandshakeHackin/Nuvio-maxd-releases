# Nuvio Max'd 0.5.34.1 — temporary issue #3 test build

Install `NuvioMaxd-0.5.34.1-native.ffpfsc` using the same manual installation method as previous test builds, or use the packaged `PPSA99288.zip` with a compatible installer. Title ID remains PPSA99288. This candidate has not been published to GitHub releases or the Homebrew Store.

1. Confirm Settings shows 0.5.34.1. Test the same source that previously failed with “Invalid data” using Native, then EVO.
2. Select VLC and test a video that already plays correctly with Native. Check red/green tint, dialogue/audio, subtitles and seek/stop/reopen.
3. If VLC falls back to software, allow startup to finish and check whether a picture appears.
4. After reproducing any problem, use Settings → Playback log, then collect the playback and renderer logs before restarting the app. Include the player/source name and whether Native works with the same source. Remove URLs, tokens and account details before posting.

For this app the typical log locations are `/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt` and `/mnt/sandbox/PPSA99288_000/download0/hui/dev/app.log`. Sandbox suffixes and FTP access can vary. Copy the log immediately after reproducing the failure in the current app session.

The automated checks validate HLS probing, software decoder selection, colour-plane packing and memory fallback/ownership. They cannot certify the exact affected PS5 output. HTTP 403, DNS failures and missing provider-script HTTP 404s are separate failures; this candidate does not claim to fix those.

The broader local libVLC 3.0.23 network regression timed out after completing local-file playback and seeking/reopen checks. VLC remains experimental; this candidate is not a fully validated network-player replacement. The exact failed run is retained in the validation receipt.

This temporary build is distributed through the issue-testing branch, rather than the normal release feed or store. PS5 numeric package contentVersion is 05.000.035; the visible app version is 0.5.34.1.
