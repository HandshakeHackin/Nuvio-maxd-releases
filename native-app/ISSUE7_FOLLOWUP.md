# Issue 7 playback and history follow-up

Included in 0.5.34 source; not included in the existing 0.5.33 downloads. Affected-console confirmation remains pending.

## Reporter evidence

[Playback log](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6253227197): Native HLS opens many inputs but closes with zero decode calls, zero pictures and zero decoder errors. Hardware initialization succeeds. The log does not contain the master playlist or its rendition grouping, so it does not establish the exact source of the stall. Final request failures may be cancellation; do not label them upstream HTTP errors without status evidence.

[Missing Continue Watching report](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6253252186): profile 1 imports 114 readable progress entries and rejects one unreadable entry. Entries include episode history and are deduplicated per title. The logged 20 library items are not a count of missing titles.

## Candidate fixes

- Match preferred/saved audio to the selected HLS video's AVProgram. A language preference must not pick an unrelated rendition group. Shared audio groups and ungrouped inputs remain supported. Only compatible tracks are exposed as switchable; stale incompatible selections are ignored.
- Stop reading inactive audio playlists. Track metadata remains available and a valid track switch enables the new stream on the demux thread. This also avoids downloading unused audio alongside the selected video.
- Build the full Continue Watching list independently of the 50-card catalog-page budget. Preserve ordering, deduplication, explicit removals, metadata enrichment and Next Up entries.
- Convert percentage progress to a resume time when a valid duration is available and no positive position was supplied. Preserve an actual positive position over percentage hints.
- Infer legacy units without rescaling explicitly suffixed position/duration fields. Previously, a `position_ms` of 3000 paired with a legacy duration of 7200 seconds became 3,000,000 milliseconds.
- Accept valid short-video durations when importing history. This does not change the minimum-duration rule for uploading new playback progress. Malformed rows still block replacement snapshots, preserving unreadable cloud data.

## Reproduction and validation

- Before the HLS fix, a local two-program master with English audio for the selected video and preferred Hebrew audio in another program selected the unrelated Hebrew stream. The corrected player selects compatible audio, produces a first picture, filters incompatible track choices and rejects a stale incompatible switch. The fixture uses the actual Native player, FFmpeg demuxer and HTTP input. Additional checks cover shared audio groups, audio-only input and ordinary-file track compatibility.
- Three new account tests fail against the previous implementation: percentage resume imported as zero, mixed units scaled 1000 times too far and an explicitly timed short clip omitted. All 80 account/profile tests pass after the changes.
- The production history builder test fails at 120 distinct titles before removing the cap and passes afterward. It also checks ordering, metadata, guest history, removal filtering and Next Up deduplication.
- Changed Native playback, WatchSync and UI source files pass PS5 target syntax checks. Full playback/seek regression results are recorded in `NEXT_REVISION_VALIDATION.json`.

These defects are reproducible, but the fixes are not yet confirmed against the reporter's exact stream or missing titles. Percentage-only progress without a usable duration still cannot provide an accurate time-based resume position. Do not infer that all 114 history rows should become separate cards or that every buffering failure has the same cause.
