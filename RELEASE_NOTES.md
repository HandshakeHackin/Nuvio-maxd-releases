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
