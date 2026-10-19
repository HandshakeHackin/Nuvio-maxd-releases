# Nuvio Max’d 0.5.34.3 targeted test revision

## Staged provider compatibility and stream-list improvements

- Fix metadata access for JavaScript providers using the owner's TMDB registration. Missing or rejected metadata authorization now gives a useful diagnostic instead of silently returning no hosts.
- Preserve the playback headers returned by Nuvio providers, including Referer and Origin.
- Add per-profile provider text, select and toggle settings, with inherited-profile safeguards.
- Keep the loading counter visible after the first stream appears. Cache successful provider scripts briefly and show recent eligible stream results while every source refreshes in the background.
- Include metadata credits under App & Updates.

Related reports: [missing provider streams #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6277799728), [loading progress and repeated searches](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6277804390). The owner registration passed a live read-only TMDB check. All four unmodified reported provider scripts returned fixture streams with required playback headers and correctly reported rejected metadata authorization. Console and real-site playback acceptance remain pending. Staged source only; no release or store deployment yet.

## Staged after temporary 0.5.34.2: Trakt registration

- Fix [Trakt connection issue #2](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/2): include Max'd's owner-registered public Client ID by default, so normal users can connect using their phone without registering a developer application.
- Remove the Client Secret prompt and use secret-free device-code exchange and refresh. Use `https://auth.trakt.tv/activate` for the phone/QR step.
- Restore the bundled application registration immediately after disconnect; preserve existing connected profiles' client identity and keep Nuvio sources as the default.
- Build configuration rejects Client Secrets, account tokens and API keys. SIMKL owner registration is still separate and has not been supplied.

Validation on 2026-10-19: with the owner's approval, official Trakt returned HTTP 200 for device-code exchange, read-only account validation, token refresh with a rotated refresh token, and revocation of the temporary test token. No library/history writes were made. Requests using the app's production headers also passed device-code creation and the expected HTTP 400 pending response. Deterministic host tests cover pairing, refresh, disconnected/connected registration handling and profile isolation. This follow-up is included in the 0.5.34.3 targeted test candidate; PS5 acceptance remains pending. Published 0.5.34.2 downloads are unchanged.


## Simple release notes

- Updated optional VLC playback with newer decoder fixes for interlaced video timing, repeated playback and VP9 support, pending PS5 testing.
- Fixed VLC callback lock ordering during seeks and an audio-open failure deadlock; linked the newer decoder’s picture-metadata import.
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

Distribution: targeted issue #3 test on `builds/v0.5.34.3`; existing release/store and prior test downloads remain unchanged. Hardware acceptance is pending. Numeric package revision: `05.000.037`.
