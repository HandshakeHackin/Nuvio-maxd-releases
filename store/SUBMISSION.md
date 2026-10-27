# 1.0.0.2 catalog submission

Status: validated and pushed; upstream PR creation was rejected with GitHub integration error 403. Use the [direct prefilled submission link](README.md). A maintainer must merge and deploy before the store updates.

Update Nuvio Max’d to **1.0.0.2-native-experimental**.

Adds a separate, profile-scoped IPTV VOD library with cached catalogs and resume, clearer provider failures, EVO 0.13.0, GPU-upload/VP9 improvements, complete Xtream category membership and addon/profile fixes. This is a public test release; physical-console validation remains pending.

Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.2-native-experimental

Only `apps/PPSA99288.json` changes. The ZIP, GitHub asset SHA-256 and icon are pinned to the release tag. Tagged and packaged `contentVersion` is `06.000.002`, increased from `06.000.001`.

Validation: catalog format check (147 records, zero errors/warnings), online verification of the published artifact, native/VLC CI, source/package verification and static archive scan with pyelftools, Capstone and YARA. The scan reports zero errors and four review warnings:

- Bundled updater ELF and its pending helper approval: SHA-256 `1f89d03e8b3dec5f19525df1424f2406d67762b7a2010b92951681338f6d46df`.
- Loopback loader address: `source/native-app/gl/update_channel.cpp` starts the bundled update helper on port 9021.
- The device-path finding refers to `/dev/icc_indicator`, used by optional console-light synchronization in `source/native-app/gl/console_lights.cpp`.

Matching updater source is under `source/native-app/update/` at the release tag and in the release source archive. No helper approval or other app record is changed by this PR. Maintainer review and store deployment are still required.
