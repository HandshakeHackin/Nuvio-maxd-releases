# Next Nuvio Max’d revision

Status: packaging for 0.5.34. Based on the existing 0.5.33 candidate plus the staged profile PIN keypad. Keep the current 0.5.33 test downloads immutable. Version metadata and the in-app changelog are updated for 0.5.34; existing 0.5.33 downloads remain unchanged.

## Simple release notes

- Updated optional VLC playback with newer decoder fixes for interlaced video timing, repeated playback and VP9 support, pending PS5 testing.
- Improved live-stream buffering and cancellation when leaving a stream. Preserve padded decoder pictures to avoid unnecessary CPU scaling.
- Added a bounded direct-memory pool for large VLC pictures and safer cleanup when memory release fails.
- Added automatic torrent recovery when a reader is waiting for data, with retry backoff and preserved downloaded pieces.
- Keep Continue Watching entries during tracker outages, and keep an episode marked completed after rewinding within the same playback.
- Fixed HLS audio selection across unrelated video variants and stopped downloading inactive audio playlists.
- Removed the 50-title Continue Watching cutoff and improved history imports for percentage progress, mixed time units and short videos.
- Added stronger HDR color checks for RGB and BGR output. Existing format selection remains in place.
- Includes the staged masked numeric profile PIN keypad.

## Implementation and sources

- VLC PS5 0.4.5: `third_party/vlc/UPSTREAM.json` pins the decoder module and records selected patches. Adaptive playback now honors network caching, checks a per-playback cancellation generation before opening/reading more segments, starts live buffering at three seconds instead of six, and treats clock gaps over ten seconds as discontinuities. Coded/visible dimensions remain separate for zero-offset pictures. Nonzero crop offsets retain the existing conversion path.
- Nuvio's VLC bridge marks video ready after a picture arrives, with an audio-only exception. Buffered video that produces no picture for ten seconds retries once in software using the same source, headers, pause state and pending resume target; a second failure reports an error. Hardware-failure fallback also retains a pending resume target.
- The VLC large-picture hook now has an implementation: 512 MiB pool limit, 128 MiB contiguous-memory reserve, page-aligned allocations, synchronized ownership and retained accounting until unmap/release succeed. Ordinary VLC heap fallback remains available. This is a picture-pool limit, not a total-process memory cap.
- [Stremio-Plus 0.5.11](https://github.com/LoZazaMastro/Stremio-Plus/releases/tag/0.5.11) informed the recovery and picture-memory work. Nuvio retains its own torrent cache and scheduling limits. Recovery requires a blocked, uncancelled reader or metadata waiter and thirty seconds without useful data; it refreshes discovery and retires stale active peers, with thirty/sixty/one-hundred-twenty-second retry spacing. Verified and partially assembled pieces remain intact. Allocation failures defer the picker rather than terminating the transfer. Recently useful delivery avoids whole-peer timeout penalties.
- [Nuvio TV 1.2.7](https://github.com/NuvioMedia/NuvioTVSmart/releases/tag/1.2.7) informed completion and tracker-outage safeguards. Cached progress is read only from the selected provider/profile file and is never uploaded to a fallback service. Successful remote refreshes retain their existing authority. Completion resets for a new playback.
- [ProsperoTV HDR correction](https://github.com/blackbearreloaded/ProsperoTV/commit/f984b545) was audited against our compositor. Nuvio already packs BGR when that VideoOut token is accepted and RGB when the alternate token is accepted. Production GPU tests now independently verify red, green and blue for both layouts; no unconditional channel swap was added.

## Verification and remaining console checks

- Host recovery tests exercise cancellation generations, blocked-reader/metadata gating, useful-data timing, backoff, retained pieces and aborted-read cleanup.
- Picture allocator tests inject map, unmap and release failures and exercise the reserve/cap and foreign-pointer handling.
- Decoder parser tests compile the vendored production parsers and exercise VP9 superframes/malformed lengths, H.264 progressive/interlaced headers, field selection and planar chroma conversion. They do not simulate successful hardware decoding.
- Episode, profile/account, tracker, GPU/HDR, Native network/lifecycle and optional VLC regressions are tracked in the validation receipt alongside this revision's source. PS5 target syntax checks cover modified application and VLC source files.
- Physical PS5 checks still required: repeated 4K opens, VP9 hardware capability, interlaced live audio/video synchronization, stop/start responsiveness, pinned picture-memory behavior, software startup retry, profile PIN entry and actual HDMI HDR colors/readback.
- Native remains the default. Optional VLC retains libVLC 3.0.24 with the existing FFmpeg 7 ABI; this is not a wholesale adoption of another application's FFmpeg 8 build. Do not claim universal HDR support or resolution of the reporter's buffering stall without their log and console confirmation.
- Automatic TV refresh switching, platform-specific Samsung/webOS player APIs and blanket suppression of decoder ENOMEM errors were not imported. The existing compatible refresh handling and explicit errors remain.

## Related reports

See `ISSUE7_FOLLOWUP.md` for reproducible playback/history defects, before-and-after checks and limits of the reporter evidence. These candidate fixes still need confirmation on the affected PS5.

- [Playback/provider report #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3)
- [Updater, Hebrew and subsequent feedback #7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7)
- [Log-location instructions](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6253091786)

Keep feature requests separate from original bug investigations. The upcoming release must include its normal GitHub documentation and Homebrew Store catalog submission when publication is authorized.
