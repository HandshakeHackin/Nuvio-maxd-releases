# Nuvio Max’d 0.5.33 — issue-testing build

This branch contains a candidate build for reporters in [issue #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3) and [issue #7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7). **This is not a normal release or store update.** The regular published release remains 0.5.32.

- [Download ZIP folder](testing/PPSA99288.zip?raw=1)
- [Download FFPFSC image](testing/NuvioMaxd-0.5.33-native.ffpfsc?raw=1)
- [Testing instructions and known limits](testing/TESTING.txt)
- [SHA-256 checksums](testing/SHA256SUMS)
- [Verification evidence](testing/TEST-VERIFICATION.json)
- [Candidate changes](native-app/ISSUE_FIXES_0.5.33.md)
- [Matching source](source/README.md), with dependency sources, licenses and file hashes in source/source-archive.json.

Install manually using the same format as your existing installation. Keep saved app data and an existing installer so you can return to the previous release if needed. Use **Native** playback for these checks; the optional VLC HLS EOF/replay test remains a known failure. This build does not establish that every HTTP 403, DNS, missing-script or provider timeout error is fixed.

The native image and ZIP have been deep-verified; the bundled Hebrew fonts match their pinned hashes. Targeted production network/font/lifecycle tests pass. Full account/transport/render CI passed for the runtime code before the packaging-only font-copy correction; final-source CI status is tracked separately. Physical PS5 and affected-account acceptance remain pending.

Reply in the relevant issue with version 0.5.33, results, selected player/provider and sanitized errors. Leave out account tokens, authorization headers and signed stream URLs.

The main page, release feed and store listing are intentionally unchanged while reporters test this branch. Files linked here are public; this is an opt-in testing distribution, not a private download.
