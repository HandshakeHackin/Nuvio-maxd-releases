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
