# Nuvio Max’d 0.5.34.4 — targeted playback test

Candidate fixes for [issue #8: audio-track switching](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8) and the recent-source caching request. Based on 0.5.34.3; existing provider, Trakt and updater fixes are retained. This is an opt-in test, not a normal release/store promotion.

[Download FFPFSC](test-builds/0.5.34.4/NuvioMaxd-0.5.34.4-native.ffpfsc) · [Download app ZIP](test-builds/0.5.34.4/PPSA99288.zip) · [Install and test](test-builds/0.5.34.4/TESTING.md) · [Checksums](test-builds/0.5.34.4/SHA256SUMS)

- Native audio switching captures the playback position before resetting output, resynchronizes quick-open Matroska too, and discards stale old-track packets. Pause state and pending seeks are preserved in the focused host checks. Reporter PS5/Torrentio testing is still needed.
- Eligible recent torrent sources are reused for five minutes while refreshing. Stable direct URLs last sixty seconds; signed/query URLs and resolver payloads are excluded. Reload bypasses the bounded, profile-scoped session cache.
- Includes audio-output lifetime and video-resource safeguards, plus VLC post-seek/rebuffer backports. No native HDR/4K improvement or universal playback claim.

[Validation and limits](test-builds/0.5.34.4/PLAYBACK_VALIDATION.json) · [Package verification](test-builds/0.5.34.4/BUILD-VERIFICATION.json) · [Matching source](source/)

Live TV, anime timing extensions, settings search and trailers remain staged for a separate broader release. Earlier test downloads and normal release/store feeds remain unchanged. Please report test results in issue #8; it remains open pending confirmation.
