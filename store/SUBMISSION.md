# 1.0.0.5 store delivery

[Release and downloads](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.5-native-experimental).

The [one-record catalog update](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/update-nuvio-maxd-1.0.0.5) is prepared and validated. Submission status will be recorded here after release publication and published-artifact verification.

Package contentVersion: **06.000.005**. Catalog validation passed for all 175 records. The release scan passed with zero errors, four maintainer-review warnings and one notice for the existing updater and optional console-light capabilities. No helper approvals were changed.

Subtitle sizing (#12) and Library covers (#13) are reporter-confirmed. Library search, Live TV Now/Next and series updates have automated checks; new-feature console/provider acceptance remains pending. Store listing changes require catalog maintainer approval and deployment.

## Submission description

Updates Nuvio Max'd to public release 1.0.0.5.

- Adds Library title search, provider-supplied Live TV Now/Next (Xtream/XMLTV), and history-aware new-episode badges.
- Includes reporter-confirmed subtitle size and Trakt Library artwork fixes: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12 and https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13.
- Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.5-native-experimental (stable release; tag suffix retained for updater compatibility).
- ZIP and tagged param.json agree on contentVersion 06.000.005; icon pinned to release tag. Only apps/PPSA99288.json changes.
- Full host regressions, PS5 compilation, deep image/ZIP round trips and source correspondence checks. New features still need console/provider acceptance.
- Static release scan: 0 errors, 4 warnings and 1 notice for the existing updater payload/loopback handoff and optional /dev/icc console-light controls. Helper approval was not modified; maintainer review is required.

ZIP SHA-256: `f97a8e722e71a6ac257b58674d7f78893ab7a218cbc359892c0fa2edcb2eea7d`

Updater helper SHA-256: `352525d18e65cf22a18bbd8dd2c4016814e53ea5ebe6a8ed40662603664c7c37`
