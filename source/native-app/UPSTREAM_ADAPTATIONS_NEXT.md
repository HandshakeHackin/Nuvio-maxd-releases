# Upstream adaptations staged after 1.0.0.5

Reviewed on 2026-11-02. These changes are staged for the next build. No release or store submission is created by this work. Physical-console/provider acceptance remains pending.

## Added where Max’d had a gap

- **Night Mode:** optional Off/Light/Medium/Strong linked-channel upward compression in the shared PCM output. Off is the default and preserves existing samples. A -1 dBFS peak limiter, near-silence fade and attack/release envelope preserve channel balance without lookahead or extra queued audio. Native/EVO use PCM when enabled; VLC already uses PCM. Settings → Audio; applies to the next video.
- **Bass in stereo downmix:** Off/50%/100% LFE contribution for Native/EVO, with normalized headroom. Existing users retain 50%. VLC retains its own downmix; this control does not claim to change it.
- **Provider guide clock:** Live TV → Options → Correct provider guide clock, in 30-minute steps within ±12 hours. Stored per source in the profile's existing Live TV state. Corrected selection and display use raw cached provider timestamps, so refresh and source changes never compound a correction. No catch-up control is added because Max’d has no catch-up player.
- **Chapter-derived skip buttons:** explicitly named opening/OP, recap, ending/ED and credits chapters can supply skip intervals. Ordinary chapter names and post/mid/after-credit scenes are protected. Intervals are 5–240 seconds, need at least two valid chapters, and respect the existing skip setting. Explicit opening wins over a generic Intro. Chapters take priority per category for series; movie API timings retain priority when available. Late timing updates preserve skip suppression by category. Settings → Playback.
- **Preferred-language trailers:** explicit Trailer language or follow audio/subtitle language preferences. TMDB requests the preferred language and English fallback, then ranks valid trailers and deduplicates candidates. At most two video-list calls plus one necessary ID lookup; cancellation/profile/detail guards remain. Settings → Playback.
- **Subtitle auto-sync:** optional and off by default. Native/EVO compare external cue timing with already decoded embedded text cues. One bounded worker, ≤4096 target / ≤2048 reference cues, ±180-second offset, ≥20 matches across three sections, ambiguity rejection and a two-second analysis budget. Common film/PAL rate corrections require at least ten minutes of reference covering 75% of the target timeline. Partial reference windows allow constant delay only. No video/speech upload or second stream read occurs. No reference, bitmap-only reference, different cuts, sparse/ambiguous matches and VLC retain manual timing. Manual timing changes suppress later auto-sync for that track during playback. The original subtitle file/cues are preserved; correction is playback-local. Settings → Subtitles.
- **Subtitle rendering contention:** the media reader now hands bounded subtitle packets to the renderer without taking the libass rendering mutex. A 256-packet / 32 MiB queue bounds backlog; seek and teardown clear it. This avoids holding up audio/video demux during expensive ASS rendering. Rendering itself remains on the existing render path.
- **libass hardening:** retain 0.17.5 plus three checksum-pinned upstream patches for empty codepage input and size overflows. Updated PS5 library hashes and recipe marker prevent silently reusing an unpatched overlay. Original ISC notice is included in future packages.

## Experimental media servers

Jellyfin and Emby support was delivered in the 1.0.0.6 tester. Plex support is staged next on `builds/v1.0.0.7`, using device-JWT linking, accessible-server discovery, the shared media-server library UI and automatic token renewal. See `PLEX_VALIDATION.json` for the separate Plex checks. See [setup, scope and interoperability limits](MEDIA_SERVERS.md) and its separate `MEDIA_SERVERS_VALIDATION.json` receipt. The earlier adaptation receipt remains historical evidence for its pinned implementation commit, rather than a claim that all of its source hashes describe this later media-server change.

## Existing fixes retained without reimplementation

- Progress writes are already asynchronous; failure is logged and does not stop the player (`watch_save_progress`). Live TV and trailers already skip normal resume/scrobble history (`records_history`).
- SIMKL already uses activity-based response caching; tracker I/O runs on workers with profile guards, retry/outage handling and bounded pagination. The Android per-frame tracking architecture that caused the reported spikes is not used here. Existing tracking contract tests remain green.
- Library artwork loads independently with bounded queues; the Smart TV Tizen DOM-paint fix does not apply to native OpenGL.
- Chapter navigation, HEVC colour fallback, source caching, seek recovery and stable playback completion are already present.
- Existing EVO/OpenGL/SDK/Stremio-Plus releases are current for the reviewed pins; no version churn. Android Media3/MPV, webOS audio gates and DOM compositing are not imported into the native PS5 player.

## References

- [Nuvio TV beta.9](https://github.com/NuvioMedia/NuvioTV/releases/tag/1.1.0-beta.9)
- [Night Mode #4366](https://github.com/NuvioMedia/NuvioTV/pull/4366), [LFE downmix #4367](https://github.com/NuvioMedia/NuvioTV/pull/4367)
- [NuvioTVSmart 1.3.0](https://github.com/NuvioMedia/NuvioTVSmart/releases/tag/1.3.0), [1.3.1](https://github.com/NuvioMedia/NuvioTVSmart/releases/tag/1.3.1)
- [ProsperoTV guide correction](https://github.com/ThatGuyKalvin/ProsperoTV/commit/d8410e5b4bd6)
- [libass patch provenance](third_party/libass/README.md)

The C++ features are adapted to Max’d's existing player/session boundaries. Subtitle auto-sync is an independent conservative timing matcher, not a claim of full Android/Smart TV engine parity. See UPSTREAM_ADAPTATIONS_NEXT_VALIDATION.json for executed checks.

## Following dependency maintenance

The next-build branch also includes the 2026-11-03 XML, image, font, FFmpeg and HEVC-layer maintenance described in [DEPENDENCY_UPDATES_NEXT.md](DEPENDENCY_UPDATES_NEXT.md), with its own validation receipt. Earlier receipts remain pinned historical evidence.
