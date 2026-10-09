# Native Nuvio Max'd for PS5

The interface uses BlackBearReloaded's `ps5-homebrew-ui` (Prospero UI), with
spring-driven carousels, artwork crossfades and translucent player panels. Series
have a selectable season row and a horizontal episode carousel with 640×360
thumbnails, descriptions and watch-progress indicators. Films show their poster
beside metadata and stream cards. These layouts follow the artwork-focused
approach in [Stremio-Plus](https://github.com/LoZazaMastro/Stremio-Plus).

This folder contains the Nuvio Max'd source for the current **v0.5.9 native experimental** app. The OpenGL
interface, addon requests and player run together in one process and are
packaged as a single FFPFSC image.

For downloads, installation, debrid setup and Watch Progress settings, start
with the [main README](../README.md). This guide covers how the native app works,
how to build it and what the tests verify.

## Accounts, addons and Watch Progress

QR pairing saves the login in app storage and refreshes it when needed. Temporary
network failures preserve the session; a revoked refresh token requires signing
in again. Pairing and token expiry use server time so an incorrect console clock
doesn't immediately expire a valid code. Signing out clears the account caches.

Enabled standard addons and the saved library load from Nuvio's **primary
profile**. Cached addons remain available during a sync outage. Manifest,
catalog, metadata, search, stream and subtitle requests keep the addon's
configuration path and query parameters intact.

Watch Progress uses Nuvio Sync for **profile 1**, through
`sync_pull_watch_progress`, `sync_push_watch_progress` and
`sync_delete_watch_progress`. Movie and episode resume positions, duration and
completion are shared with other devices using the same account, primary
profile and Nuvio Sync progress source.

Progress is written every 30 seconds during playback, on pause and on stop.
Remote progress is checked at startup/sign-in, every minute while idle and
through Settings → Watch Progress. Offline updates are saved in an account-scoped
queue, with a two-minute retry backoff or a fresh attempt after restart.

Newer remote updates win conflicts. Pushes merge the profile snapshot to retain
other devices' entries; an empty or unreadable response never triggers an empty
replacement. Removing a title from Continue Watching uses the delete RPC.
Only portable progress fields are uploaded. Stream URLs, playback headers and
addon credentials aren't included. Guest progress stays local; secondary
profiles and Trakt/Simkl progress sources aren't supported yet.

## Browsing and playback

Home catalogs follow Nuvio's `showInHome` and required-search rules. Other
required extras can use addon defaults. Loading or empty Home/Addons pages keep
the left menu accessible, and catalog errors are logged separately from empty
results. The reported blank AIOMetadata home page remains under investigation.

The stream picker has addon filters, dark rounded cards and size, language,
video and audio badges. L1/R1 switches addons, Up from the first stream opens
the filter row and Triangle refreshes results. Filtering keeps the original
playable stream index. Badges come from addon text and filenames; size can also
come from `behaviorHints.videoSize`. Custom image-badge rules aren't synced.

Direct HTTP/debrid links and their required headers go to the built-in player.
Raw torrents use the integrated engine only when **Peer-to-peer torrents** is
enabled in Settings; the default is off. A direct URL takes priority when an
addon also includes a torrent hash. Account and addon services need internet
access, but you don't need a separate local Nuvio server.

In v0.5.9, common decoded YUV formats keep their native dimensions. The app copies
decoded planes before the hardware buffers are reused, uploads them to the GPU
and converts them into a native-sized RGB10_A2 surface (8-bit RGBA for uncommon
software formats). The converted YUV picture retains 10-bit precision until
final composition. Compatible HDR10 pictures now use experimental 10-bit
BT.2020/PQ output; SDR scanout stays 8-bit. Display fitting preserves
cinema crops and sample aspect ratio. Low-aligned 10-bit hardware output and
high-aligned P010 are handled explicitly, including the logged 3840x1606 crop
that previously produced a green picture in the fallback path.

The hardware decoder releases idle codec workspaces when switching codecs.
H.264 and HEVC Main10 setup get a bounded depth-1 retry if the larger pipeline
fails, with failed allocations released first. The Main10 retry adapts EVO
Player's memory fallback within this app's player. The full EVO app isn't bundled.
Parallel downloads share a cooldown and failure budget; terminal failures cancel
workers, and DNS/resource failures have bounded retries.
Parallel DNS failures have a separate three-failure budget, preventing serial
failures from taking the longer retry path used for a temporarily busy server.

The default **Display output → Follow PS5** reads the full console resolution
before EGL opens and selects 1080p, 1440p or 2160p. The temporary VideoOut handle
is closed before EGL starts. Explicit overrides apply after reopening the app.
A failed probe or display request falls back to 1080p with an on-screen notice;
it never changes the decoded source dimensions. Render size and the console's
startup configuration are reported separately. Use the TV's signal information
to confirm the actual HDMI resolution.

The frame loop follows the display backend's accepted 60/120 Hz rate, limiting
extra submissions when swap returns early. This does not change the source
frame rate or add motion interpolation. Timing logs restart after pauses and
buffering so they measure active playback.

`player: presenting` logs the texture dimensions, bit depth and alignment.
`player: quality` and the player controls show picture/render dimensions, color
conversion, accepted refresh rate and stereo output. `display:` records startup
resolution selection.
Presentation errors stop playback with a message. Videos above 4096 pixels per
side are rejected.

Settings → HDR playback defaults to **Native HDR10 (experimental)**. Compatible
PQ/BT.2020 YUV pictures bypass SDR tone mapping. The final shader packs exact
BGR10A2 pixel words into the existing BGRA8 EGL attachment, and VideoOut reads
those words as 10-bit BT.2100 PQ. Blending, dithering and sRGB conversion are
disabled for that final pass. This uses the pinned OpenGL SDK without changing
its buffer registration or context ownership.

Controls use a separate premultiplied sRGB overlay, converted to BT.2020 and
composed in linear light at 100-nit white. Mode changes drain GPU work and the
flip queue before changing the registered buffer attributes in place. Refused
HDR or missing post-flip HDR confirmation stops playback with a message. SDR
conversion requires the explicit **Convert HDR to SDR** setting. If returning
to SDR is refused, the app keeps packing HDR pixels while it retries, so menu
pixels aren't interpreted in the wrong format.

Native HDR is not yet console-verified. Source mastering-display/content-light
metadata are not forwarded. Dynamic HDR10+ metadata, HLG and complete Dolby
Vision processing aren't implemented; Dolby Vision profile 5 has no compatible
HDR10 base and is rejected in native HDR mode. Audio is decoded to stereo,
including Atmos-labeled sources. DRM, arbitrary JavaScript plugins and
external-player-only links aren't supported.

The target firmware range is **9.00 and above**, where the required jailbreak,
kstuff-lite and ShadowMount+ work. **Firmware below 9.00 needs testing.** This
range describes intended support; it has not been verified on every version.

Console reports on firmware 11.20 confirm earlier builds' startup, QR sign-in,
1080p playback and HEVC Main10 decoding. v0.5.9 corrects known 10-bit packing,
resolution, H.264 setup and DNS retry problems, but recent console feedback still
reports choppy 1080p HEVC and haze or moving background pixels in otherwise
smooth 4K playback. Their cause remains unconfirmed. Live account sync and other
firmware versions remain unverified. See [playback quality](PLAYBACK_QUALITY.md)
for the video/audio path, equipment limits and the remaining HDR checks.

## App updates

Settings → App updates uses the public download feed and a bundled payload
helper to replace the FFPFSC after the app exits. This path needs ShadowMount+
1.7 and still needs a console test. See [UPDATING.md](UPDATING.md).

## Build on Linux or WSL

Use the versions and checksums in [dependencies.json](dependencies.json).
You'll need LLVM/Clang/lld 18 or newer, g++, zlib development headers, Python 3,
git and zip, plus these components:

1. **PS5 payload SDK 0.43**, including its playback and networking libraries.
2. **ps5-native-app-boilerplate** at the recorded commit. Build
   `build/host/ps5-native-tool`, reproduce `runtime/libc.prx` with
   `tools/rebuild-libc.sh` and check the runtime checksum.
3. **ps5-homebrew-ui** at the recorded commit. Run `tools/fetch-opengl-sdk.sh`
   to download and verify the pinned OpenGL SDK.
4. **MkPFS**, set up with the boilerplate's
   `tools/setup-packaging-dependencies.sh ffpfsc`.

From the repository root, set the paths to your toolchain and build:

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
export BOILERPLATE_DIR=/path/to/ps5-native-app-boilerplate
export GL_SDK=/path/to/ps5-opengl-sdk-1.0.0/sdk
export LLVM_CONFIG=llvm-config-19
export MKPFS=/path/to/ps5-native-app-boilerplate/.deps/MkPFS/.mkpfs-run-linux
bash native-app/gl/build.sh PPSA99288 "Nuvio Max'd"
```

Set `COMPILER_RT` if LLVM puts `libclang_rt.builtins-x86_64.a` outside its resource
directory. `JOBS` defaults to 4.

The build also compiles `update/helper.cpp` as a payload and bundles it as
`nuvio-updater.elf`. It never needs to be downloaded separately.

The build produces `native-app/dist/PPSA99288/`, `PPSA99288.zip` and
`PPSA99288.ffpfsc`. The FFPFSC wraps an exFAT filesystem in a PFSC-compressed PFS
container, with the app files at the inner filesystem root. Licenses and
`build-info.json` travel with the app. Effective exFAT mount permissions still
need checking on the console.

## Run the host checks

Install g++, pkg-config, OpenSSL/curl development packages, the FFmpeg CLI and
avcodec/avutil/avformat development packages, EGL/GL development packages and
Mesa. From the repository root:

```sh
bash native-app/tests/run.sh
```

For an optional UI preview using the production login, film, series, home and playback screens:

```sh
bash native-app/tests/preview.sh
```

The v0.5.9 suite has **168 tests and 27 display-selection assertions**:

| Area | Checks | What they cover |
| --- | ---: | --- |
| Account and HTTPS | 46 | Pairing, saved sessions, refresh, addons, catalogs, progress import/push/delete, conflicts and offline retry |
| Navigation | 6 | Empty/loading pages, catalog arrival and normal card navigation |
| Stream cards and series | 12 | Addon formatting, filters, season selection, episode focus and late artwork |
| Episode playback | 34 | IntroDB timing validation, skip targets, post-credit scenes, countdown, cancellation, season rollover and direct/debrid quality matching |
| Decoder setup | 10 | Simulated platform failures, allocation cleanup, codec switching and bounded retries |
| Video packing, GPU presentation and pacing | 19 | 8/10-bit planes, crops/strides, SDR conversion, exact 10-bit HDR packing, UI luminance, native 4K detail and 60/120 Hz scheduling |
| HDR output transitions | 7 | Refused switches, flip-queue drain, post-flip confirmation, timeout, SDR restoration and handle ownership |
| Display selection | 27 assertions | Full output vs pane dimensions, 1080p/1440p/4K selection, overrides, failed queries and handle cleanup |
| Network | 6 | Reads/seeks, recovery, shared cooldown, cancellation and bounded DNS retries |
| FFPFSC updater | 28 | Release selection, title/path/hash checks, cancellation, mounts, disconnected storage, verified backups and atomic replacement failures |

Account checks use production C++ and curl against a local HTTPS fixture.
Decoder setup, display selection and HDR transition checks simulate PS5 platform
calls. GPU checks use Mesa, including
an actual H.264 High clip with B-frames and a one-pixel 3840x2160 checkerboard
through the UI renderer, plus all 1,024 levels in packed HDR output and HDR
controls at 100-nit white. Network checks use a local server and injected DNS
failures. These checks don't establish PS5 GPU performance, live Nuvio account
sync, personal debrid-provider connectivity or firmware compatibility.

Release validation fully extracts the image, compares every packaged file hash
and checks the native SELF and title metadata. See the
[release guide](../ps5/GITHUB.md) for the verification command and asset names.

## Source and licenses

The native UI and player are adapted from
[unofficial-stremio-ps5-port](https://github.com/Sp9nky/unofficial-stremio-ps5-port)
at commit `a4b12fb515a3044f073f4befba0dd90d8244eddd`. Nuvio branding and protocols
come from [NuvioTVSmart](https://github.com/NuvioMedia/NuvioTVSmart).
This is an unofficial client. See [LICENSE](LICENSE) and
[THIRD_PARTY.md](THIRD_PARTY.md) for component details.
