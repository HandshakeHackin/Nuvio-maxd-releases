# Max’d 1.0.0.1 — Live TV imports, multiscreen and audio controls

- Fixed large Live TV imports reported in [issue #10](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/10). Channel lists now support up to 100,000 channels and 128 MiB. Xtream channels are parsed individually to reduce temporary memory use; authentication and category metadata retain smaller limits. Xtream VOD/series libraries are not imported.
- Added experimental four-screen Live TV. Open Live TV’s sources/filters menu → Four-screen TV, then choose four different channels from the current page/filter. D-pad focuses a tile, Cross listens to it, Square replaces it, Triangle opens it full-screen, and Circle exits.
- Added PCM volume/boost from 50–300%, with 100% as default. Boost can distort loud peaks; compressed bitstream audio is never amplified.
- Added audio sync from −2 to +2 seconds and subtitle sync from −10 to +10 seconds. Positive audio delay makes sound later relative to the picture; positive subtitle delay shows captions later. Defaults persist in Settings → Audio/Subtitles.
- Added playback menu controls: Left/Right selects Audio/Subtitles; L2/R2 adjusts sync in 50 ms steps; L1/R1 changes Audio volume; Triangle resets the selected column. Active bitstream playback needs PCM before live boost adjustments.

## Four-screen testing limits

Your provider must allow four simultaneous connections. Start with SD channels. The grid uses independent software previews up to 640×360, one audio output, and bounded queues; sources must be SDR and no larger than 1920×1088. Four HD feeds may exceed console performance. 4K/HDR sources are rejected in the grid; normal full-screen playback retains Native, EVO and VLC options.

Host playback, gain, sync, import and grid-control tests passed. The PS5 build and installer checks passed, including matching image/ZIP contents and source hashes. Four live streams, console GPU/CPU performance and Bluetooth sync still need physical-console confirmation. This is a public test release; please report results in issue #10 with sanitized playback logs if problems remain. Keep a known-working installer for rollback.

Package revision: 06.000.001; title ID: PPSA99288. Preserve existing app user data when updating. Homebrew Store availability follows catalog approval and deployment.

---

# Max’d 1.0.0

- Added playback badges for resolution, HDR/SDR, source audio channels, codecs and release types such as WEB-DL, Blu-ray and DVD-Rip.
- Added Live TV with M3U and Xtream Codes, channel search, groups, favorites and recent channels.
- Added anime intro/credits timings through AniSkip and optional Anime-Skip.
- Added settings search and a Watch trailer action on movie/show details.
- Recent sources appear faster while fresh results load in the background.
- Fixed audio-track changes losing playback position or pause state.
- Fixed seeking back after playback ends getting stuck buffering; added clearer queue/seek diagnostics.
- Improved audio/video cleanup and optional VLC seek, HEVC recovery and HLS shutdown.
- Includes Trakt phone pairing, provider, profile, history and updater improvements.

The affected tester confirmed the audio/source fixes in [issue #8](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8#issuecomment-6294939778). Provider compatibility work relates to [issue #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3); tracker and updater work relates to [#2](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/2) and [#7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7). Anime timings adapt [community PR #6](https://github.com/HandshakeHackin/Nuvio-maxd-releases/pull/6).

Live TV requires your own playlist/server. Anime-Skip is optional and requires a client ID; AniSkip does not. Some trailers are unavailable when their host requires unsupported playback methods. Native is the default player; VLC remains optional. HDR output depends on the console/display path.

Version 1.0.0. The legacy tag suffix is retained so older in-app updaters can discover this release. Host and package checks passed; console acceptance of the combined new features remains pending. Homebrew Store availability follows catalog approval and deployment.


---

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

---

# Nuvio Max'd 0.5.32 — MAX'D icon and updater shutdown fix

## Changed

- New MAX'D family app icon: angular M/play mark, prominent MAX'D lettering and smaller NUVIO label, using Nuvio teal, white and pink/purple. The PS5 app, GitHub page and tagged store icon use matching artwork.
- Guarded a missing episode-task cancellation flag during shutdown. Closing the app before starting an episode task could crash before the updater's exit handoff, causing its helper to cancel installation. This is a candidate fix for the updater portion of [issue #7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7), pending confirmation on the affected PS5.

## Important installation guidance

If Install and close crashes in your current version, install 0.5.32 manually from GitHub, or through Homebrew Store once its listing shows 0.5.32. Keep your saved app data. A newly downloaded image cannot repair the updater code still running in an older installation. The automatic image installer requires a compatible FFPFSC install, payload loader and ShadowMount+ API; folder installs use store/manual replacement.

The fresh-launch shutdown regression reproduced a segmentation fault before the fix and passes afterward. All 28 updater transaction tests pass. Full CI and native package verification evidence are included in native-validation.json. These checks do not prove PS5 helper/mount/exit behavior; affected-console confirmation is still needed.

## Existing features and open reports

The 0.5.31 provider-download and multiple-profile Continue Watching fixes are retained. Reporters confirmed those improvements; [#4](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/4) is closed. [#3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3) remains open for the remaining difference in provider/source coverage compared with desktop. No additional provider-coverage change is included in 0.5.32.

Hebrew characters displaying as question marks, also reported in [#7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7), remain under investigation and are not fixed in this release. Tracker setup limits and experimental HDR/EVO/VLC behavior remain as documented; Native is the default player.

See the [testing guide](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/builds/v0.5.32/native-app/TESTING-0.5.32.md). Please reply in the relevant issue if the problem persists, with the version and sanitized logs. Do not post passwords, account tokens, PINs or stream URLs.

---

# Nuvio Max'd 0.5.31 — Continue Watching, trackers and playback fixes

## Fixed

- Continue Watching keeps valid entries when a profile's sync response contains an unreadable record. Readable entries remain cached; incomplete responses cannot infer deletions or replace cloud history. Candidate fix for [issue #4](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/4#issuecomment-6249838477).
- Provider script downloads use the PS5's direct nonblocking socket setter, avoiding a curl path that can report Unsupported protocol (curl 1) before any HTTP response. Candidate fix for the attached log in [issue #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6249405789); affected-console confirmation is still needed.
- Fixed a race during recovery from a failed compressed-audio output port to PCM.

## Added

- Trakt, SIMKL and MDBList connection options under Settings → Account & Services → Tracking accounts ([request #2](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/2)).
- Separate Library Source and Watch Progress settings. Defaults remain Nuvio Library / Nuvio Sync; only connected services become selectable, and connecting one does not change your selections.
- Per-profile tracker connections, source choices, resume caches and offline retry, with temporary Nuvio fallback when a selected service is unavailable. Trakt/MDBList libraries use watchlists; SIMKL uses its tracked library. Custom external lists are not included.

## Playback changes

- Prefer BGR HDR10 and change displayed buffers in place, retaining RGB fallback for rejected formats. Removed the unregister/re-register fallback.
- Improve low-frame-rate scheduling for Native/EVO when Prefer 120 Hz is accepted, resetting on pause, buffering, seeks and output changes. VLC does not use this scheduler.
- Protect video textures with GPU fences before overwrite/teardown while retaining owned decoder-frame copies.

The playback work adapts [ProsperoTube](https://github.com/blackbearreloaded/ProsperoTube), with source/license notices retained. These are experimental improvements, not a confirmed solution to every HDR rejection, judder or audio issue. Native remains the default; EVO and VLC remain optional.

## Setup and testing

Trakt/SIMKL phone pairing currently requires registered application credentials configured in their menus; shared app registrations are not bundled. MDBList connects with a personal API key. See [tracking setup](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/builds/v0.5.31/native-app/TRACKING-0.5.31.md) for connection/import options and limits. Never share keys or tokens in issue reports.

76 account/HTTPS/profile/sync tests pass, including the reproduced partial-snapshot failure. An approved real Nuvio account's profile-scoped resume reads matched its cloud data; it contained no unreadable records and did not reproduce the reporter's failure. Read-only account checks do not establish PS5 UI, PIN, cross-device write or playback acceptance.

Targeted provider, HDR/pacing/GPU, software-GL rendering, EVO and audio-recovery checks passed during development; full release CI evidence is included in native-validation.json. The native build resolves all 412 imports, and all 146 packaged files match extraction and ZIP verification. PS5/reporter retesting remains necessary. Please reply to the linked issue if the problem persists, with version and sanitized logs.

Keep existing app data when updating. The release includes the install ZIP, FFPFSC image, matching source archive, checksums and testing notes. The Homebrew Store catalog update is submitted separately; availability follows maintainer merge/deployment.
