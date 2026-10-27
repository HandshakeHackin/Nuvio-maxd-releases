# Max’d 1.0.0.2 — unpublished combined test build

Includes the staged IPTV Library and provider-availability messages, plus the
applicable October 27 upstream updates. This is a test candidate; PS5/GPU/HDMI
results are not established by compilation or host tests.

## Changes

- Update all 15 embedded EVO decoder/presentation inputs to the exact 0.13.0 tag
  (2db44b5294657b12bf91666154a05f029ddb5aa2), with upstream hashes/licenses.
- Add Native VP9 Profile 2 hardware selection (10-bit, 4K level 5.1), bounded
  depth retry and the existing software fallback if the console refuses it.
- Adapt Stremio-Plus 0.5.20's rectangle sampled-plane approach. YUV uploads avoid
  the pinned PS5 driver's CPU tiling path; conversion output remains 2D/RGB10_A2.
  Restore both texture targets, preserve fences, resolution and colour handling.
- Retain every known Xtream category membership, with a bounded 128 IDs per
  channel, one channel identity and no duplicate playback/history entries.
- Refresh saved account addon manifests on a worker, with a best-effort eight-
  second budget and three-second per-request cap. Failures retain saved metadata.
- Clear the retained search query when changing profiles, alongside the existing
  result/task-generation reset.
- Update UI paragraph measurement/drawing to use the same wrapped lines.
- Build with payload SDK 0.45 tools/headers/stubs and the unchanged pinned
  PacBrew 0.40.2 media libraries, in an isolated SDK directory.
- Keep verified OpenGL 1.0.2/MkPFS 1.1.0 and use available dependency mirrors.
- Add the profile-local IPTV Library: conditional sidebar, movies/series,
  category search, episode/season selection, cached catalogs and private resume.
- Explain provider HTTP failures, unavailable titles and access/connection limits
  without treating decoder or PS5 output failures as provider-wide outages.

## Audited equivalents and scope

- EVO's standalone audio-lead limiter and thumbnail scrub controller are not
  linked into Max’d. Our own submitted-PCM clock and seek flow remain; seeking
  does not wait for a separate thumbnail preview. Cadence checks include 8/12 fps.
- Existing HDMI-CEC/Media Remote input remains enabled and is regression-tested.
- VLC PS5 0.6.0's decoder source matches the embedded decoder except our retained
  unknown-dimension rejection and direct-memory workspace fallback. Preserve
  these tested fixes. libVLC stays 3.0.24 and shares Native/EVO's FFmpeg ABI.
- VLC's standalone interface, equalizer UI and renderer-specific upscalers are
  not bundled. This update does not claim those features or an entire VLC app.
- Existing profile-local tracker/source state remains. MDBList API-key support
  is retained; Android/web device pairing cannot be enabled just by copying
  another application's OAuth registration.
- ShadowMount+ 1.7 beta8 is a separate console payload/installation. It is not
  embedded, installed or changed by this app image. Newer SDK offsets do not
  establish support for running the app on an untested console firmware.

## Console checks

Compare 1080p/4K SDR and HDR sources, 8/12/24 fps, VP9 Profile 2, seek/pause and
repeated Native/EVO/VLC changes. Verify picture colours, subtitles, audio sync,
UI/art after playback, HDMI-CEC, IPTV categories/cached resume and provider-error
messages. Native HDR still depends on console firmware/output acceptance.
