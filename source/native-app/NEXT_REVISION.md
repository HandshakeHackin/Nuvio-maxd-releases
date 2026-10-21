# Included in 1.0.0: playback badges

- Add compact outlined source badges beneath the playback title: resolution, HDR/SDR, selected audio channels, release type, codec and known 10-bit picture precision.
- Recognize 480p/576p/720p/1080p/1440p/4K/8K and other common resolutions, WEB-DL/WEBRip/Blu-ray/BDRip/BRRip/DVD-Rip/DVD-SCR/HDTV/CAM/TS/TC and REMUX.
- Decoded picture dimensions and selected-track labels override filename hints. Cropped 3840×1600 pictures remain 4K. HDR10 source badges can coexist with an HDR-to-SDR output line; Dolby Vision/HDR10+ metadata labels explicitly say source.
- Refresh on track/picture changes. VLC source channel counts are displayed separately from output/downmix settings. Unknown details remain absent; row overflow stays within the controls.
- Source and UI checks are recorded in PLAYBACK_BADGES_VALIDATION.json. Playback badges are part of the 1.0.0 candidate; package verification is recorded with the rebuilt installer.

# Nuvio Max’d 1.0.0 release candidate

- Live TV: M3U playlists and Xtream Codes, channel groups, search, favorites and recent channels, isolated by profile.
- Anime timings: free AniSkip and optional Anime-Skip alongside IntroDB, with safer episode mapping and timing validation.
- Search settings by name, description or category.
- Watch trailers from movie and series details, without adding trailers to history or trackers. Supported public YouTube responses and direct HTTPS/HLS trailers are resolved on demand.
- Faster return to recent sources: bounded profile-specific memory cache with background refresh and a Reload bypass.
- Fix Native audio-track changes losing position or pause state; retire packets from the previous track.
- Fix seeking back after playback ends getting stuck buffering: retire the old EOF state before releasing the seek gate. Add queue/seek diagnostics and a deterministic regression test.
- Playback safeguards for audio buffer lifetime, GPU cleanup and optional VLC seeking/HEVC recovery.
- Retain Trakt phone pairing, provider compatibility, profile, history and updater fixes from the 0.5.34 test series.
- Accept stable release tags in the updater while preserving support for existing download formats.

The affected tester [confirmed the 0.5.34.4 audio/cache test was working](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8#issuecomment-6294939778). This does not establish console acceptance of every new 1.0 feature.

## Setup and limits

Native remains the default player. Live TV accepts user-supplied sources; this version does not include EPG, recording, catch-up or Xtream VOD. AniSkip needs no key; optional Anime-Skip requires a client registration. Trailers are manually selected; authenticated or unsupported YouTube formats may be unavailable. Cloud YouTube playback validation was blocked by HTTP CONNECT 403 and remains unverified. Universal HDR output is not claimed.

## Release preparation

App version: **1.0.0**. Independent PS5 package revision: **06.000.000**, greater than 05.000.038. Delivery branch: `builds/v1.0.0`.

For the first public 1.0 release, retain `v1.0.0-native-experimental` as the compatibility tag so installed 0.5.x updaters can discover it. The release title can be 1.0.0 and the GitHub release can be stable; the historical suffix is only an updater compatibility requirement. New 1.0 clients also accept `v1.0.0` and `v1.0.0-native` tags.

This candidate is prepared locally. Public GitHub release and matching Homebrew Store catalog submission remain rollout steps; neither is implied by compilation.

## Earlier implementation and validation record

## Targeted audio-track test (issue #8)

- Capture the playback position before replacing the audio output clock. Resynchronize indexed and quick-open Matroska inputs at that position, preserve pause state and pending seeks, and reject packets from the previous track generation.
- Log the selected track and resynchronization anchor without exposing stream URLs. A failed output reopen or indexed seek stops with an actionable error instead of continuing with conflicting clocks.
- Host coverage exercises two-track MP4 and quick-open Matroska over custom HTTP AVIO; affected-console Torrentio/P2P validation remains required.

# Unreleased: returning to recently found sources

Addresses the [source-list caching request](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6284415813), also confirmed in [issue #8](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8#issuecomment-6288657099).

- Reuse validated torrent source descriptors in session memory for up to five minutes; resolve/play them through the current profile's existing torrent/debrid path.
- Keep safe direct HTTP results for sixty seconds and continue excluding signed/query/fragment playback URLs and credential-bearing client resolver payloads.
- Show eligible cached choices immediately with Refreshing sources feedback while fresh source requests run; fresh empty/error results still replace old source results.
- Scope results by profile, title/episode, source identity/version/name and provider options. Use bounded memory and least-recently-used eviction across eight searches.
- Reload deliberately bypasses the current cached search. Cache entries are not saved to disk or carried across profile changes.
- Audio-track-switching report [#8](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8) remains a separate unresolved playback issue.

---

# Unreleased: on-demand trailers

- Add **Options → Watch trailer** on movie and series details, with a list of available trailers and teasers.
- Find trailers from addon metadata and the app's own TMDB registration, ranking official English trailers first and removing duplicates.
- Play HTTPS video/HLS trailers through the selected Native/EVO/VLC player; resolve supported public YouTube MP4/HLS responses on demand without storing expiring links.
- Keep trailers out of watch history, resume progress, watched marks, tracker updates and episode autoplay. Leaving the title or switching profile cancels pending results.
- Manual playback only. Some YouTube videos require unavailable signatures, authentication or other playback methods and return an unavailable message.
- Staged alongside the other next-build changes; no new package or public/store release yet.

---

# Unreleased: playback stability

- Backport VLC's post-seek frame-discard reset and reference-safe HEVC rebuffering from Stremio-Plus 0.5.18. Genuine seeks still flush decoder references; late-clock recovery does not.
- Reject damaged FFmpeg-decoded pictures rather than presenting them after rebuffering.
- Keep two aligned audio output buffers alive through drain/close; reject queueing after output closes and prevent unsafe PCM/bitstream port reuse when shutdown fails.
- Detach obsolete video framebuffer textures before releasing them on format/size changes; clear partial allocation state when GPU storage fails. Retain native picture resolution and existing HDR precision.
- Fetch the unchanged, checksum-pinned OpenGL SDK 1.0.2 from its maintained LedyBacer fork.
- Stop failed audio restoration with a clear app-restart message rather than retrying PCM recovery indefinitely.
- Staged alongside settings search, anime timings and Live TV. Physical PS5 acceptance and a new packaged build remain pending.

See [playback changes and validation limits](PLAYBACK_STABILITY.md).

---

# Unreleased: settings search

- Add Search settings at the top of Settings. Search names, descriptions and categories without case sensitivity; multiple words narrow the results.
- Select a result to open or change the original setting, with the same profile and sign-in safeguards as category navigation.
- Show a helpful message when no settings match; cancelling leaves settings unchanged.
- Staged alongside Live TV and anime timings; not packaged or published.

---

# Unreleased: anime skip timings

- Add free AniSkip and optional Anime-Skip intro, recap and credits coverage for anime, adapted from [community PR #6](https://github.com/HandshakeHackin/Nuvio-maxd-releases/pull/6).
- Correct IMDb episode numbering supplied by anime catalogs and add conservative Kitsu/MAL/AniList mappings.
- Validate timings against the playing file before selecting a preferred source; preserve valid fallback timings when a release differs.
- Fetch optional and free timings independently; keep credentials and pending results isolated by profile and playback.
- Retain the staged Live TV work below. This source adaptation is not a new published build.

See [Anime timing setup and safeguards](ANIME_TIMINGS.md).

---

# Unreleased: Live TV

- Add a dedicated Live TV tab with M3U playlists and native Xtream Codes server/username/password setup.
- Add channel categories, name search, favorites, recent selections, saved sources and paged channel browsing.
- Keep source credentials and channel preferences local to each profile; cancel pending source requests on profile changes.
- Use the selected player with sequential live playback and short buffering; keep live channels out of movie/episode history and trackers.
- Accept Xtream/XUI M3U session metadata instead of misclassifying the whole channel list as an HLS stream.
- Support extensionless live HLS segments with HTTP-only nested inputs; keep live buffering and recovery at the short target.
- Owner-supplied service testing imports 1,624 channels through both Xtream and M3U, with 39 Xtream categories; real channels from both source types passed five-second host audio/video decodes.
- EPG, recording, catch-up TV and Xtream VOD/series are outside this first version. Physical PS5 testing remains pending.

See [Live TV setup and validation](LIVE_TV.md). Staged after the 0.5.34.3 test build; no new package/version or public/store deployment yet.

---

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
