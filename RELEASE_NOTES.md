# Nuvio Max'd 0.5.34 — playback recovery, profile PINs and watch history

## Changes since public 0.5.32

- Bundled Hebrew font fallbacks for menus, title details and subtitles. The 0.5.33 test reporter confirmed Hebrew text and Torrentio content display correctly.
- Fixed relative HLS segment addresses after redirects and added clearer errors when a source returns a web page. Hidden torrent results now have an explanation without automatically enabling peer-to-peer.
- Added a masked, controller-operated numeric keypad for profile PINs, avoiding the PS5 system keyboard's suggestion/history path. Server-side PIN verification remains in place.
- Fixed Native HLS audio choices across unrelated video variants and stopped reading inactive audio playlists. This is a candidate fix for the buffering report in [#7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6253227197), not confirmation for every live-stream/provider stall.
- Removed the 50-title Continue Watching cutoff and corrected imports for percentage progress with a known duration, mixed seconds/milliseconds and short videos. Addresses [missing titles feedback](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6253252186); exact-account confirmation is pending.
- Preserved cached Continue Watching during tracker outages and kept episodes marked completed after rewinding within the same playback.
- Added stalled torrent recovery with retry backoff while retaining downloaded pieces.
- Updated optional VLC's PS5 decoder integration to upstream 0.4.5 fixes, including repeated playback, interlaced timing and VP9 handling. Improved live buffering/cancellation, bounded large-picture memory, coded/visible picture handling and a one-time software retry when buffered video produces no picture.
- Fixed VLC callback lock ordering during seeks and an audio-output-open failure deadlock; added the newer decoder’s picture-metadata import.
- Added independent RGB/BGR HDR color checks. Existing HDR pixel-format selection remains; this is not a new universal HDR fix.

## Test this version

Install **0.5.34**, then use **Native** first to retest the same stream that stalled. Also compare Continue Watching using the same profile and watch-progress source as mobile, and try the profile PIN keypad. Optional VLC is under **Settings → Video → Playback engine**; changes apply to the next stream. VLC remains experimental and requires PS5 testing.

If an old updater crashes, manually replace the installation while preserving saved app data. Use `PPSA99288.zip` for folder/store installs or `NuvioMaxd-0.5.34-native.ffpfsc` for compatible image installs. Existing 0.5.33 test downloads remain unchanged. Homebrew Store availability depends on catalog merge/deployment; verify its displayed version before installing.

Host tests exercise real FFmpeg/HTTP HLS rendition selection, playback, audio switching, seeking, EOF/replay, redirects and cancellation. All 80 account/profile tests and the production Continue Watching history checks pass. Native PS5 build, package verification and release CI results are recorded in the attached validation report. These checks do not replace testing on a physical PS5. Native remains default; EVO and VLC are optional. Tracker application credential requirements remain unchanged.

Please reply in the relevant [issue](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7) with version 0.5.34 and your results. If a stall persists, attach a fresh sanitized playback log after reproducing it; keep passwords, PINs, tokens and signed stream URLs out of reports. Additional provider-coverage differences tracked in [#3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3) are not all resolved by these changes.

VLC validation note: the current libVLC 3.0.24 CI playback suite passes, including audio-output failure injection. An initial CI attempt and a local libVLC 3.0.23 HLS reopen/close stress run timed out; these results are retained in validation evidence. Do not treat optional VLC as a confirmed fix for every stall.
