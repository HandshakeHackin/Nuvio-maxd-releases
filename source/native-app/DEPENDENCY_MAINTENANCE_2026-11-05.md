# Dependency maintenance — 2026-11-05

Unpublished maintenance candidate based on experimental 1.0.0.10, commit
`dc7b71234461a0baf7d0fb8d14e7d18ca3bdadfc`. No new public version has been
allocated. Existing releases, store records, downloads and earlier checkouts are
preserved. Work branch: `maintenance/dependency-refresh-20261105`.

## Changes

| Component | Previous | Candidate | Purpose |
|---|---|---|---|
| Provider HTML selectors | cheerio-without-node-native 1.0.0-rc.2 | Cheerio 1.2.0 slim | Remove the vulnerable nth-check 1.x chain without exposing Node network APIs |
| HarfBuzz | 15.0.0 | 15.1.0 | Font shaping maintenance |
| WebP / SharpYUV | 1.4.0 | 1.6.0 | Artwork decoder/encoder maintenance |
| libxml2 | 2.13.5 | 2.15.4 | Current XML maintenance release, tested with the reader API used by VLC |

The old Cheerio module names still resolve to the same maintained implementation.
The lockfile and browser bundle were regenerated without npm install scripts.
The new bundle and generated dependency notices reproduce byte-for-byte.
`npm audit` reports zero known advisories for the resolved lockfile at this date;
this is not a native-library or undiscovered-vulnerability clearance.

The XML cross-build exposed the absence of getentropy in the PS5 SDK. A small,
checksum-pinned patch seeds libxml2's hash randomization through SDK
arc4random_buf, avoiding the time/address fallback. The archive's unresolved
randomness import is arc4random_buf, not getentropy.

All target headers, static archives and pkg-config metadata from the overlays
are recorded in dependencies.json (522 outputs). Sources and recipes are also
checksum-pinned. The earlier libass hardening patches remain enabled.

## Playback compatibility review

EVO 0.13.5 was reviewed at `f6e43e6561796630673f1e20eb1eb67d6428514c`.
The integrated clock, playback code and decoder API headers are unchanged.
The decoder differences introduce Dolby Vision per-shot peak mapping, requiring
additional helpers and renderer support. Those changes are not applicable as
standalone bug fixes to this app and were not imported. The existing verified
HEVC enhancement-layer filter remains in place. See third_party/evo/LOCAL_CHANGES.md.

FFmpeg stays at 7.0.3 across Native, EVO and VLC. Clean recompilation in the new
work directory initially changed the informational tool paths embedded in the
FFmpeg configuration string. The build now normalizes only that string to the
existing reproducibility receipt prefix; actual compilation uses the selected
SDK. The previously verified libavcodec.a and libswscale.a hashes match exactly,
as do the other existing FFmpeg output pins. No FFmpeg major migration or
renderer/output-format change is included.

## Checks completed

- Full PS5 app compile/link, including VLC core and 98 static plugins; target
  imports resolved (414 functions from loaded modules).
- All 522 target pins, base SDK/OpenGL pins and provider bundle/lock checks.
- Existing complete tests/run.sh suite, including playback, subtitles, artwork,
  HDR fallback, controller mappings, torrent streaming, network behavior,
  profiles/persistence and updater failure handling.
- Production QuickJS provider tests, including module aliases, HTML/XML parsing,
  nth selectors, a long malformed nth expression, network/cancellation and
  provider credential/setting isolation.
- Separate Jellyfin/Emby media-server and Plex fixtures.
- Upstream native tests: 8 libxml2 tests, 1 Expat test group, 37 libpng tests.
- Actual updated host libraries: XML tree and VLC-style callback reader APIs,
  malformed XML/PNG rejection, lossless WebP round-trip and severe truncations,
  and Latin/Hebrew/Arabic/Thai/CJK shaping/rasterization with bundled fonts.
- XMLTV guide regressions against updated host libraries.
- Provider bundle and license-notice reproducibility; npm security audit.
- YAML parsing, Python/shell syntax and whitespace checks; upstream tag monitor.

Logs and final package verification evidence are retained under
`/workspace/nuvio-toolchain/dependency-refresh/`. The final package is rebuilt
from the committed checkout so build-info.json identifies clean matching source.
Container/ZIP verification compares all extracted bytes and checks native SELF
and updater metadata. See verification.json and zip-verification.log there.

Host fixtures do not establish physical PS5 playback, controller or display
acceptance. Console testing of artwork, subtitles, provider selectors, seeking,
HDR fallback and the three playback backends is still required before making a
new public release. No measured performance improvement is claimed.

## Ongoing checks and remaining migrations

A weekly workflow checks key native release lines and npm advisories;
Dependabot proposes npm and GitHub Actions updates. These become active only
when merged into the default branch. Nothing is automatically upgraded or
published. The native tag monitor is a version check, not a complete CVE scanner.

Later batches can assess MiniUPnPc and compression-library updates, newer torrent
changes, Mesa/HDR backports and FFmpeg 7.1. Those remain separate compatibility
projects; replacing PacBrew wholesale could replace customized PS5 dependencies.
OpenSSL remains on its current 3.5 maintenance line.

## Reproduce

Use the pinned base SDK and a new directory outside it:

```sh
bash tools/build_maintenance_libraries.sh BASE_SDK NEW_MAINTENANCE_DIRECTORY
export PS5_PAYLOAD_SDK=NEW_MAINTENANCE_DIRECTORY/sdk
bash third_party/provider-runtime/build.sh
python tools/validate_build_dependencies.py "$PS5_PAYLOAD_SDK" "$GL_SDK"
bash gl/build.sh
```

Set the existing documented BOILERPLATE_DIR, GL_SDK, MKPFS and private provider
application configuration for packaging. Optional MAINTENANCE_COMPONENTS accepts
exact versioned media components for iterative rebuilds; the parent always
checks every pinned output, so missing or stale dependencies still fail closed.
Host library tests use tests/maintenance_runtime.sh NEW_MAINTENANCE_DIRECTORY.

Detailed changed target hashes and EVO file comparisons are recorded in
DEPENDENCY_MAINTENANCE_2026-11-05.json. Original FFmpeg rebuild evidence remains
in DEPENDENCY_REBUILD_1.0.0.8.json.
