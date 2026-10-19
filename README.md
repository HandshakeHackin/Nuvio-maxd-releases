# Nuvio Max’d 0.5.34.2 — temporary test build

Targeted updater and Trakt pairing corrections, based on 0.5.34.1.

[Testing instructions and downloads](test-builds/0.5.34.2/TESTING.md) · [Changes and limitations](RELEASE_NOTES.md)

The updater now waits for the backing image after unmount and offers a handoff test with persistent diagnostics. Trakt device-code handling and configuration reporting are corrected, but ordinary-account pairing still requires an owner-registered Max’d Trakt application, which is not bundled in this test. Do not create developer credentials just to use the app.

The normal public/store release remains 0.5.34. No normal release tag or store promotion is created for this temporary test.

[FFPFSC](test-builds/0.5.34.2/NuvioMaxd-0.5.34.2-native.ffpfsc) · [ZIP](test-builds/0.5.34.2/PPSA99288.zip) · [SHA-256 sums](test-builds/0.5.34.2/SHA256SUMS) · [Verification and limits](test-builds/0.5.34.2/ISSUES_2_7_VALIDATION.json)

150 packaged files and 725 source hashes verified. Updater/Trakt focused checks passed; broader playback CI has unresolved failures. PS5 testing is required.
