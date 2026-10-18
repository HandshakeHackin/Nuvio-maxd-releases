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
