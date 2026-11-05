# Torrent Edition comparison and integration

Compared official Nuvio Max’d 1.0.0.11 (`795aefdefd0bb4b12d010550d60bd5994bb5a338`)
with the latest chewlord76/nuvio-torrent-edition main at
`c8837a9e5f431ddf21b3aedb532f37c6923175d6`. The previous imported baseline was
`8e34590582ad89c0f12182289f83c2526bd47c4a`.

This is an unpublished development candidate on `integration/torrent-upstream-refresh`.
No public version, tag, main branch or store record is changed by this work.

## What changed upstream

Four commits after the imported baseline change three implementation files:
`session_torrents.cpp`, `session_watch.cpp` and `player.cpp`. The apparent large
diff is mostly line-ending churn. The actual features are resume/restart menu
controls, a buffering-stage adjustment, head-only probing for every torrent,
and suppressing reopen behavior after torrent seek failures. The native
BitTorrent engine files did not change in this interval.

## Adapted and refined

| Area | Candidate behavior |
|---|---|
| Resume | Play the most recently watched valid file, using its exact file index and saved position. Square offers Resume, Play this file from beginning, Choose file and Remove. |
| Restart | Clear only the selected file’s resume point. Other episodes/files and profiles retain their progress. Cancelled or stale-profile menu callbacks cannot clear progress. |
| Metadata/file selection | Automatic magnet selection gets its real file identity before resume is applied. The temporary unknown-file ID never saves progress as file zero. |
| Unknown duration | Save usable progress even when a streaming container does not report duration; clear completed progress on EOF. Ignore nonfinite, finished and malformed saved points. |
| Startup | Short head-only probes for identified MKV/EBML and TS/M2TS inputs starting at zero. MP4 remains seekable for tail moov metadata; explicit resume retains index access. |
| Buffer status | Show filling-buffer status only after metadata and actual downloaded progress, rather than treating a connected peer as downloaded data. |
| Seek cancellation | A new seek interrupts a blocked torrent read. Index-search/recovery reads have a five-second deadline per attempt. |
| Failed seek | Restore a confirmed demux timestamp before continuing playback, flush stale queues and show a notice. If restoration fails, stop with a clear error. Never automatically reopen in a loop. |
| Critical errors | Decoder recovery and audio-track resynchronization failures remain errors; they are not hidden by the generic torrent fallback. Startup resume errors cannot be overwritten by Playing state. |
| Stream input | Strict hash/index URL parsing, checked seek arithmetic and EOF bounds prevent malformed indices or signed offset overflow. |

## Retained protections

Torrent streaming remains off by default with console confirmation. Web Drop
remains paired, temporary and bounded, with submission confirmation on the PS5.
Existing tracker-thread limits, cancellation, cache bounds, profile isolation,
HDR-to-SDR fallback and the 1.0.0.11 dependency pins are retained. No renderer,
HDR output, library dependency or EVO version migration is included.

The upstream approach was not copied wholesale: disabling seeking on every
container can break tail-moov MP4; selecting the first matching saved key can
resume a different episode; clearing every resume entry loses unrelated file
progress; returning early for all torrent seek errors can hide decoder/audio
failures or continue from a damaged demux position.

## Verification

- Expanded real engine/session fixtures: per-file resume/restart, stale-profile
  callbacks, unknown duration, invalid saved values, pending file identities,
  paired Web Drop, bounded reads, seek interruption, strict URLs and overflow.
- Production Player + real FFmpeg, using a deterministic file-backed torrent
  AVIO boundary: MKV, TS, M2TS, front-moov MP4 and tail-moov MP4 startup/resume/
  seeking; partial-I/O failed seeks, rollback, bounded index reads, no reopen
  loops, preserved decoder/audio errors, startup failure and port cleanup.
- Existing production HTTP/HLS/network and playback lifecycle regression suites.
- Full PS5 app compile/link and package verification are recorded in the workspace
  evidence directory `/workspace/nuvio-toolchain/torrent-refresh/`.

The player fixture does not simulate a real public swarm; real engine waiting
and cancellation are exercised separately. No throughput benchmark, “instant”
80-GB startup guarantee or universal crash immunity is claimed. Physical PS5
and real-peer testing remain pending. Index-read deadlines do not limit ordinary
buffering while waiting for missing playback pieces; that wait is cancellable.

Upstream GPL attribution and exact reviewed file hashes are retained in
`source/native-app/app/licenses/torrent-edition/UPSTREAM.json`.
