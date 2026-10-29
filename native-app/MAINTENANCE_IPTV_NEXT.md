# Maintenance libraries and saved IPTV navigation — unreleased

Staged on `builds/next-playback`, after source commit `a3b2ccc`. This batch joins
that branch’s artwork limits, HEVC colour recovery, track restoration and chapter
navigation. It does not change the public release version or publish an installer.

## Dependency changes

- libass 0.17.3 → **0.17.5**, keeping the existing font shaping dependencies and
  bundled fonts. Host tests run the updated library, including positioned ASS,
  Arabic/CJK/Thai shaping, external subtitle seek and bitmap subtitle clearing.
- curl 8.18.0 → **8.23.0** and OpenSSL 3.5.2 → **3.5.9**. Keep the supported
  OpenSSL branch, `/app0/ca-bundle.crt`, app-owned PS5 sockets/nonblocking hooks,
  and the existing compression/PSL support. FFmpeg’s ABI remains unchanged.
- Vendored nlohmann/json 3.11.3 → **3.12.0**, checked against its release digest.
- Preload the VideoDec2 system module before the first EGL display. Keep compute
  and codec queries after graphics initialization, at playback time. This adapts
  the relevant startup ordering insight from ProsperoTV commit
  `fb12d4326fbd69080a9215ab5592f28365fe93a9`; it does not introduce a second
  display or an early decoder warm-up. Physical older-firmware testing is pending.

`dependencies.json` records source digests and exact installed archive/header/PC
hashes. OpenSSL build metadata uses a fixed `SOURCE_DATE_EPOCH` and a path-independent
compiler name. Build validation rejects an older or mismatched SDK dependency set.
The source downloads, build recipe and license texts are available for rebuilds;
the original SDK is retained unchanged.

### Building the isolated SDK overlay

Use the pinned Payload SDK 0.45 / PacBrew 0.40.2 base and the LLVM toolchain already
required by the app. Set `LLVM_CONFIG` if it is not on PATH.

```sh
bash native-app/tools/build_maintenance_libraries.sh "$BASE_SDK" "$MAINTENANCE_WORK"
export PS5_PAYLOAD_SDK="$MAINTENANCE_WORK/sdk"
# Use the usual BOILERPLATE_DIR, GL_SDK, provider config and VLC build environment.
bash native-app/gl/build.sh
```

The builder verifies the unmodified base and all downloaded archive digests,
builds into staging directories and replaces individual overlay files. It never
installs into or truncates files in the original SDK. Completion markers allow
resuming; use a fresh work directory after changing recipe flags. Installed
output hashes were recorded with LLVM 19 and the stated recipe; a different
compiler/configuration must be reviewed before changing output pins. The VLC
private dependency view refreshes its SDK symlinks so it cannot silently retain
older libass or networking archives after an overlay upgrade.

## IPTV Library navigation

- Choose **A–Z** in the filter row, or **Options → Browse A–Z**. Only populated
  initials appear. Common accented Latin names group with their base letter; numbers
  and other scripts group under **#**. Choose **All** to clear the filter.
- The category’s normal provider order stays intact; an active letter groups
  its matching titles alphabetically. Episode/season order remains unchanged.
- Triangle/search or **Options → Search saved catalogs** searches saved movie
  and series catalogs across the active profile’s imported IPTV accounts.
  Results show the provider and whether the title is a movie or series. Long
  provider badges fit the poster width and retain the movie/series prefix.
- Playback and series expansion select the result’s provider before creating
  the authenticated request. IPTV progress remains provider- and profile-local;
  addon catalogs, Nuvio watch history and connected trackers remain separate.
- **Circle**, **Return to browsing**, or an empty search returns to the current
  provider/category. Choosing server/type/category exits saved-search mode.
- Search works offline from existing private caches. It does not download every
  provider’s catalog. Open categories or All categories and let them load to
  populate more saved titles; a saved result does not guarantee provider uptime.

Search runs off the UI thread, checks cancellation while reading, deduplicates
source/type/title identity across overlapping saved categories, and ignores
removed accounts, episode caches, invalid headers, corrupt files and symlinks.
The scan is bounded to 128 MiB / 512 cache files and 2,000 hits; limited searches
ask for a narrower query. Profile/source changes and newer searches invalidate
older callbacks. The existing 100-title UI pages and private cache storage limits
remain in effect.

## Validation and limits

See `MAINTENANCE_IPTV_NEXT_VALIDATION.json` for exact checks and hashes. Host
regressions and PS5 compile/link/import validation establish implementation and
integration coverage. They do not establish PS5 HDMI/audio output, old-firmware
startup behavior, controller feel or real subscription-provider availability.
Those require console testing before a public release.

Host account/network regressions use host networking libraries; the updated PS5
networking archives were compiled, linked and checked for the required socket
hooks/imports, but have not been executed on a physical PS5. The host subtitle
checks explicitly loaded libass 0.17.5. UI previews also passed ASan/UBSan with
leak detection disabled in the managed ptrace environment. RGB copies of the
preview frames were inspected to avoid image-viewer alpha interpretation.
