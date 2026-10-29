# Next maintenance build — playback improvements

Staged October 29, 2026, after public 1.0.0.2. No release number, published
installer, store submission or physical PS5 validation is implied by this file.

## Included changes

- Bound artwork requests to 64 queued jobs, decoded upload staging to 16 images /
  32 MiB, and cache metadata to 4,096 entries. Producers wait for capacity and
  wake on shutdown. Reject compressed images over 32 MiB or source RGBA images
  over 64 MiB before allocating a decoded bitmap. Existing HTTP response limits,
  resized disk artwork and the 640 MiB GPU texture budget remain. Up to four
  workers may each hold a bounded decode/resample in progress, and the current
  upload batch is separate from the 32 MiB staging budget. This is not a 32 MiB
  total artwork-memory claim. Shutdown releases queued pixels and GPU textures.
- Recover missing Native/EVO HEVC colour fields from the base-layer SPS/VUI in
  Annex B or hvcC extradata. Preserve explicit container colour/range fields.
  The reader bounds input, NAL count, SPS size, bit reads, Exp-Golomb values and
  reference-picture loops. Truncated or unsupported metadata leaves existing
  behaviour intact. HDR is not inferred from bit depth or filenames; Dolby
  Vision compatibility/output checks remain. This does not change native HDR
  scanout acceptance or add Dolby Vision output. In-band-only SPS not extracted
  into codec extradata is outside this fallback.
- Preserve the current audio stream (including a pending selection), pause,
  runtime PCM boost and audio delay across the necessary Native/EVO seek reopen.
  Expose reopen generations to rebuild in-file subtitle IDs, retaining the
  selected track by metadata rather than a stale menu index. Explicit Off and
  downloaded external cues remain. An unavailable/ambiguous selected subtitle
  falls back to Off instead of another language. VLC retries retain its current
  audio/subtitle choices and sync settings, with newer manual choices taking
  precedence over pending startup restoration. This does not add a new engine
  failover policy or share track choices between different titles/profiles.
- Read embedded chapters through FFmpeg for Native/EVO and libVLC for VLC.
  Normalize, sort, deduplicate and bound snapshots to 512 entries; reject
  invalid/out-of-duration timestamps. Show a Chapters column only when at least
  two valid chapters exist. Options/Up opens the menu; Left/Right selects the
  column, Up/Down selects a chapter and Cross jumps through the normal seek path
  while preserving pause. Show spaced chapter markers and the current chapter
  name with playback controls. Live TV and four-screen previews do not expose
  chapters. IPTV VOD can use chapters supplied by its media file.

## Upstream review

- [NuvioTVSmart 1.2.10](https://github.com/NuvioMedia/NuvioTVSmart/releases/tag/1.2.10),
  commit d8f61c044d1fd7f4dc5f2305ed0fdaa8d52197e5: artwork lifecycle review.
  Its DOM, observer and webOS changes are not imported into the PS5 renderer.
- [NuvioTV beta.8](https://github.com/NuvioMedia/NuvioTV/releases/tag/1.1.0-beta.8):
  HEVC SPS colour fallback and chapter navigation as design references.
- [NuvioTV track-preservation work](https://github.com/NuvioMedia/NuvioTV/commit/7dc661731b69acaf1c8b007f63421c38896fafde):
  recovery audit; independent adaptation to the existing PS5 player lifetime.

## Validation and console checklist

See `UPSTREAM_PLAYBACK_NEXT_VALIDATION.json` for recorded checks. Host tests use
real FFmpeg/libVLC demux/decoding and mocked console audio/GPU boundaries; host
rendering uses Mesa software GL. Compilation/linking does not establish console
behaviour.

On PS5: browse a large set of catalogs, leave and return, switch profiles, then
play 1080p/4K SDR and HEVC HDR content with/without container colour metadata.
Confirm no new colour/range changes on correctly tagged content. Select an
alternate audio track, embedded/SDH/external subtitle or Off, adjust audio/subtitle
sync and boost, and seek repeatedly. Compare Native/EVO/VLC. Check a movie with
chapters, jump while playing and paused, scroll a long chapter list, then play a
file without chapters and Live TV to confirm that the extra column disappears.
