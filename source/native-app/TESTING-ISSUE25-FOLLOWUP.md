# Issue #25: subtitle and stream-resource follow-up

This 1.0.1.1 test follows the reporter’s Native/Torbox failure after the earlier
test: disappearing embedded captions, subtitle download timeouts, an initial EVO
crash and intermittent stream I/O errors. Other HTTPS sources worked for them.
Torbox links can themselves be HTTPS; the evidence does not identify a protocol
or provider fault. The actual console crash cause is still unconfirmed.

## Changes

- Dense embedded ASS signs/animation no longer trigger a full event flush at
  10,000 cues. Only expired embedded events are pruned. Active dialogue remains;
  events stay bounded at 10,000 per track, and excess new read-ahead is omitted
  until space is available. External subtitle files retain their timeline for
  backward seeks. External auto-sync cannot advance the embedded retention clock.
  The separate 4,096-packet / 32 MiB pending limit remains.
- Curl handles share a thread-safe DNS-address cache with the normal 60-second
  expiry. Range downloads and subtitle GETs can reuse warm console lookups.
  A subtitle retry still bypasses cached failures. No connections, cookies,
  headers or credentials are shared, and no DNS server or TLS policy changes.
- Stream reader/parallel-worker thread allocation failures return an error or
  use fewer workers. If none of the parallel workers starts, the existing probe
  reader resumes as one connection. This prevents a reproducible uncaught
  thread-allocation exception; it does not establish the reporter’s crash cause.
- The earlier subtitle GET bounds, transient retry, cancellation, certificate
  validation, profile isolation and private data handling remain in place.

## Regression checks

The old code fails a real libass dense-sign fixture and real stream tests with
thread-allocation failures. The new checks retain current dialogue through a
10,040-sign burst, clear it on time, reclaim expired events, preserve external
backward seeks, exercise concurrent streaming/subtitle requests using warmed DNS,
and verify single/fewer-worker read, seek and shutdown under thread exhaustion.
Existing transport, private-header, retry and cancellation checks remain.

Renderer tests prepare checksum-pinned host libass 0.17.5 when the distro’s older
library lacks the pruning API already present in the native app. Recovered SDKs,
downloads, previous test builds and dependency pins are preserved.

## Please test

Close Max’d before installing. Preserve saved data and the previous installer.
This unsigned test displays the expected dismissible **Unverified build** notice.

1. In Native, replay the same Torrentio/Torbox source and embedded subtitle track.
   Watch the dialogue and signs/on-screen text. Pause, seek and change languages.
2. Download an OpenSubtitles subtitle while that source is playing. Repeat with
   the previously working HTTPS source for comparison.
3. Stop playback, select EVO and replay the same Torbox source. Stop/reopen it
   several times; report any crash, I/O error or remaining subtitle timeout.

Report the version, PS5 firmware, engine, subtitle type and exact error. Do not
post private stream URLs, tokens, account files or copyrighted video samples.
The fix remains unconfirmed until console results arrive. Keep issue #25 open.
No stable release or store update is part of this focused test delivery.
