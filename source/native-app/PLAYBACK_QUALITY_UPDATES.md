# Playback quality updates in 0.5.18

Six changes implemented against the 0.5.17 source. This is an experimental build;
host tests and a PS5 cross-build do not establish console or HDMI correctness.

1. **Timing and recovery.** The media clock counts successfully submitted PCM
   frames with bounded interpolation within the current grain. Pause/underrun silence does not advance it. Seeking resets the sample
   epoch and holds audio until a target picture has been copied into the frame
   queue. Audio crossing the seek target is trimmed at sample precision. Runtime
   decoder failures in both video engines initiate a software decoder and seek
   back to a keyframe near the current position, with synchronized rebuffering.
   If that source cannot be sought, recovery reports an error rather than hanging.
2. **Audio.** Optional surround output negotiates float 7.1, S16 7.1, float stereo,
   then S16 stereo. The resampler matches the accepted format/layout; the quality
   line reports the real PCM output. This is decoded PCM, not Atmos/DTS:X bitstream
   passthrough. The toggle applies to the next stream. Device channel routing and
   firmware 11.00 float/surround support require console verification.
3. **Subtitles.** A mutex-protected libass adapter preserves authored ASS/SSA
   positioning, colours and timing; synthesized text uses user styling. Bitmap
   tracks are decoded by FFmpeg, copied before AVSubtitle is freed, and rendered
   inside the fitted video rectangle. PGS clear events end prior pictures.
   Bitmap history is bounded by count and bytes; seek flushes embedded events,
   while external files survive. Subtitle texture lifetime stays on the UI thread.
4. **Language coverage.** Bundle OFL-licensed Noto Sans CJK SC (static regular TrueType) and Noto Sans Thai.
   Text subtitles retain logical order for HarfBuzz/FriBidi shaping in libass;
   the legacy UI fallback still uses visual ordering. CJK/Thai faces are explicitly
   requested for synthesized subtitle tracks and are also in UI glyph chains.
5. **HDR compatibility investigation.** Keep RGB10A2 PQ first. Only the native
   invalid-format result permits a BGR10A2 PQ retry. The successful format controls
   compositor bit packing; no layout change occurs on refusal. Read output status
   before/after switching and expose compact diagnostic evidence. Acceptance alone
   is not HDMI HDR confirmation. Both formats were rejected in the user's previous
   firmware 11.00 logs; this change does not promise to fix that console. Refusals
   continue to require explicit selection of HDR-to-SDR playback.
6. **Memory/buffering.** A 256 MiB refcounted compressed-packet slab replaces many
   payload allocations. Codec references can outlive a queue flush or slab owner.
   Full/fragmented slabs retain the original FFmpeg packet; logs expose fallback
   counts. Normal packet budget is 256 MiB, with bounded 320 MiB headroom only to
   find packets for a starving stream, and a 65,536-packet guard. PCM uses a fixed
   two-second ring rather than an ever-growing vector with front erases.

The implementation uses our native UI and new local adapters. No browser service
or upstream application code was copied. Bundled library/font notices travel with
all binary and source releases. Release notes describe the changes to users.

## Validation and console follow-up

Run `tests/run.sh`, including `playback_quality.sh`, and the GPU preview/tests.
The new regression suite covers negotiated PCM/fallback, pause/seek sample epochs,
ring overflow/wrap, codec-held slab references, reuse, styled ASS, Arabic/CJK/Thai,
PGS palette conversion and clearing. HDR boundary/GPU tests cover alternate-format
acceptance and matching word order. Validate source, staged image and release zip.

On a PS5: verify per-channel routing with a known channel-identification file;
check 24/23.976 fps at 60 and 120 Hz; seek repeatedly in HEVC/H.264, including a
long GOP; switch audio tracks; check subtitles after backward seek; test native
HDR then SDR restoration on firmware 11.00 and a newer firmware. Collect quality,
shown fps, drops, packet pool counters and HDR evidence, without stream credentials.
