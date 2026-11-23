# Nuvio Max'd 1.0.0.12 experimental

## Player and Live TV test build

- Open Up / Options, then Square for playback options.
- Switch between the current title's loaded sources while keeping playback position.
- Choose another video file in an active built-in torrent. Each file keeps its own progress; file lookup is cancellable.
- Change subtitle size, font, colour, effect and background while watching; preferences are saved per profile. Bitmap subtitles cannot be restyled; authored ASS/VLC may retain their own styling.
- Adjust audio delay by ±0.10 or ±0.20 seconds, or reset it. Existing 0.05-second track-menu adjustments remain.
- Show reported container bitrate for Native playback, or measured input rate for VLC. Missing values are labeled unavailable; these are different measurements.
- Enable the optional channel/time grid in Live TV → Sources & filters → Guide layout. Browse a two-hour window through available listings for the next day. Cross tunes the live channel; future listings do not provide recording or catch-up.

## Other changes since public 1.0.0.11

- Optional Sports Hub with per-profile sport, competition, team and region selections; disabled by default. Uses configured playback sources.
- Source preferences and ranking controls, plus torrent resume, buffer telemetry, recovery and diagnostics improvements.
- Updated and rebuilt whatwg-url provider runtime, verified bundle hashes and production URL regressions; refreshed CI checkout.

## Testing and installation

This is a prerelease for console testing, not a confirmed fix for every setup. Host player/guide tests, UI previews and package verification are recorded with the release; physical PS5 confirmation is pending. Keep the previous working installer and preserve saved user data. Use PPSA99288.zip for a folder install or NuvioMaxd-1.0.0.12-native.ffpfsc for an image install; fully close the app before replacing it. Do not overwrite a mounted image. Content version: 07.000.012.

For issue #10, test source switching/resume, torrent file selection, subtitle appearance after restart, Bluetooth audio delay, and guide navigation/tuning with and without listings. Report app version, firmware, selected player, install type and exact steps. Remove provider credentials, tokens and stream URLs from logs.
