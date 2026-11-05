# Torrent experience development candidate

Implements the five requested improvements on `integration/torrent-upstream-refresh`, based on official 1.0.0.11. This work is unpublished.

- Resume shows the actual selected filename and saved time, with separate Resume, Start over and Choose another file actions. Restarting one file leaves other files’ progress intact.
- Launch stages distinguish peer discovery, metadata, buffering and player startup. Transfer speed, verified cached data, contiguous bytes ready ahead and compressed playback-buffer seconds are separate measurements; bytes are not converted into guessed playback time.
- Playback failures offer appropriate retry, beginning and alternate file/source actions. Saved progress survives failures. Profile, service and playback generation guards reject stale actions. Unrecoverable audio-port failures still require restarting the app.
- Torrent settings retain an explicit opt-in, explain cache allocation and show Web Drop state. Cleanup stops the engine and removes only its regular cache file, refuses active readers and symlinks, and preserves torrent metadata and watch progress.
- Diagnostic summaries use fixed engine/codec/error categories and a validated version. URLs, filenames, account details, tokens and raw errors are excluded. Users can read the summary, scan a plain-text QR and copy on a phone, or save `diagnostic-summary.txt` locally. No console clipboard API or diagnostic web listener is introduced.

Validation includes production Session workflow tests, actual engine cache/read-ahead tests, redaction tests, real Player/network regressions, software-rendered screen previews and a full PS5 cross-build. Physical PS5 playback, controller interaction and real public-swarm behavior remain unverified. Evidence and packaged candidate: `/workspace/nuvio-toolchain/torrent-ux/`.
