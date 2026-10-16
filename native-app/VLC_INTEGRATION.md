# Optional VLC engine — 0.5.28 test build

Settings → Video → Playback engine offers Native, EVO and VLC 3.0.24. Native
remains the default. A saved choice applies to the next stream. VLC is linked
inside this application; it does not launch another installed title.

## Implementation

- VLC 3.0.24 and selected modules, adapted from ZiZc3/VLC v0.4 at
  d46be6a68f4244466b041c2dc1b640915ac1a983, are built against the **existing SDK
  FFmpeg 7 ABI**, shared with Native/EVO. No second FFmpeg ABI is linked.
- Owned, aligned NV12/P010 callback buffers feed the existing OpenGL renderer.
  Frame ownership and seek generations protect the renderer from recycled
  decoder buffers. Color tags, peak luminance, visible dimensions and sample
  aspect ratio cross a small private C bridge.
- The port's PS5 H.264/HEVC decoder is available when the console decoder module
  loads. Hardware failure permits one reopen with libavcodec software decoding.
  The compute queue is prepared on the UI thread only when VLC is selected.
- VLC's amem output feeds the existing bounded PCM output at 48 kHz. Stereo
  downmix uses VLC's channel converter; 7.1 channel order is explicitly mapped
  to the PS5 port. S16 is preferred, with float stereo as the device fallback.
  Queue deadlines follow pause/resume; seek/stop clear old audio. VLC currently
  uses PCM even if the saved Native/EVO setting is Auto or Bitstream.
- A custom access module uses Nuvio's authenticated HTTP input and cancellable
  reads. Adaptive HLS opens playlists and segments through that access module.
  libdvbpsi supplies VLC's MPEG-TS demuxer. Native Matroska demux uses
  libebml 1.4.6, libmatroska 1.7.2 and pinned utf8cpp 3.2.5 headers. The target module also adapts the
  built-in torrent reader; the HTTP host fixtures do not exercise torrent peers.
- Playback controls, duration, position, audio tracks, embedded subtitle choice,
  subtitle delay, watch progress and stop/reopen share the existing menus. File
  subtitle default flags are read inside libVLC without exposing its structures.
  VLC renders embedded subtitles itself; file ASS styles are handled by VLC.
  External addon captions retain Nuvio's subtitle renderer. Embedded VLC subtitle
  styling does not currently mirror every Nuvio styling preference.

## Reproduce

The host needs CMake, make, pkg-config, Python, curl and patch.
Use the existing pinned Payload SDK 0.44 and OpenGL SDK 1.0.2. Set
`PS5_PAYLOAD_SDK`, `BOILERPLATE_DIR`, `GL_SDK` and optionally `VLC_BUILD_ROOT`,
then run `bash native-app/gl/build.sh`. `tools/build_vlc.sh` fetches release
source archives by SHA-256, applies the recorded patches and builds static
libraries in a private dependency prefix. The SDK installation is not modified.
The build validates vendored source hashes and the final native imports.

`third_party/vlc/UPSTREAM.json` records source URLs, hashes, port provenance and
local adaptations. Original notices and the VLC/libdvbpsi licenses are bundled
in `licenses/vlc/`. Matching source distributions must accompany public release
artifacts, including the pinned VLC, libdvbpsi, libebml, libmatroska and utf8cpp archives.

## Validation and limits

`tests/vlc_host.sh` uses real host libVLC, real decoded frames/audio and the
production HTTP access module. It covers MP4/Matroska, authenticated HTTP/HLS,
segment transitions, audio selection, paused and backward seeks, stop/reopen,
failed inputs and cancellation during response headers and body reads.
`tests/netstream.sh` also holds a reconnect between two seeks and verifies the
returned bytes; pending requests cannot overwrite a newer requested position. Host runs use libVLC
3.0.23; the PS5 target compiles 3.0.24 and the PS5 decoder module. Native lifecycle
and HLS/seek tests also run with libVLC linked to check engine dispatch/cleanup.

A successful build and host fixtures do not establish console compatibility.
Verify launching, 1080p/4K, stereo/surround, seeking, subtitles, pause/resume and
engine switching on a PS5. Native HDR still uses the existing console output
path: adding VLC does not resolve a rejected HDR10 scanout format. HLG output
is explicitly refused rather than rendered with the wrong transfer curve.
Dolby Vision-only and unusual color profiles are not validated. VLC's copy
path and additional memory use need console performance testing.
