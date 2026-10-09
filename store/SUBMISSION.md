# Submission description

This description was submitted in [catalog pull request #109](https://github.com/blackbearreloaded/ps5-homebrew-catalog/pull/109).

---

**Title ID:** PPSA99288
**New listing, update or reservation:** New listing
**What the app does:** Nuvio Max'd is an unofficial native Nuvio client for PS5. It connects to Nuvio accounts and addons, syncs watch progress, and supports episode skipping and next-episode playback.

- [x] The app is a native PS5 app with its own title, `eboot.bin` and `sce_sys/`.
- [x] This submission comes from HandshakeHackin, the account that owns `source_repo`.
- [x] The artifact is a ZIP of the `PPSA99288/` app folder.
- [x] The artifact is attached to the published prerelease `v0.5.9-native-experimental`; the record uses that tag without its leading `v`.
- [x] The record's SHA-256 matches GitHub's digest for the exact release asset. That asset will not be replaced.
- [x] `sce_sys/param.json` at the release tag uses title ID `PPSA99288`, title name `Nuvio Max'd` and content version `05.000.009`, raised from the previous build.
- [x] The build is distributed under GPL-3.0 with its component license notices and matching source.
- [x] The review policy has been read.

Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v0.5.9-native-experimental

The ZIP is `PPSA99288.zip` (26,745,860 bytes), with SHA-256 `8d7793f4294465837a382dab9ee2db5df0b514508760809fa7cfcd57a308feda`. The 512×512 icon is pinned to the release tag. The official `catalog check` and `catalog verify PPSA99288` commands both pass without errors or warnings.

The public repository documents where the released source lives. This release includes `NuvioMaxd-0.5.9-source.tar.gz`, containing the native source, vendored UI code, build instructions, dependency pins and third-party notices. Its source manifest and the packaged build metadata identify development commit `b712587741da97989945ed7849452051984bdbea`.

For provenance review, the native UI and integrated player are adapted from [Sp9nky's unofficial Stremio port](https://github.com/Sp9nky/unofficial-stremio-ps5-port). The project adds Nuvio account and addon integration, Nuvio Sync progress, episode features and the Nuvio Max'd interface. The source and [third-party notices](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/v0.5.9-native-experimental/native-app/THIRD_PARTY.md) credit Prospero UI, EVO Player, Stremio-Plus and the other upstream components. This is not an official Nuvio, Stremio or Sony release.

The package includes an automatic update helper, `nuvio-updater.elf` (SHA-256 `109466a379c343534dd36f1e45ce7949612887528a5036e99965c88e7449dc8a`). Its source is `native-app/update/helper.cpp` in the matching archive. For FFPFSC installs, the native app hands off to this helper through the local payload loader; after the app exits it uses ShadowMount+'s public API to release the image, replace it with the verified update and request a rescan. Folder installs use manual updates. The helper is disclosed here for the catalog's release scan and maintainer review.

This is an experimental prerelease. Packaging verification, 168 host tests and 27 display-selection assertions passed; this exact build has not been verified on a console. HDR10 output and automatic image installation remain experimental. Dolby Vision/HDR10+ dynamic metadata and source mastering metadata are not forwarded, and audio output is stereo. These limits are documented in the README and `native-validation.json` attached to the release.

The official release scanner was also run against the local ZIP without executing its contents. It passed with zero errors and three warnings: the bundled helper is a payload, the main title contains the local payload-loader address, and this helper digest is not yet in the catalog's approved helper list. These are disclosed for maintainer review; this submission does not change `helpers/approved.json`.
