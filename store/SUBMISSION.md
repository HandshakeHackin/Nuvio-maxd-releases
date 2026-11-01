# 1.0.0.5 release delivery

[Download 1.0.0.5](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.5-native-experimental). This is the latest stable GitHub release. All nine published asset digests and sizes match the verified packages and source. The legacy tag suffix is retained for updater compatibility.

Library search, Live TV Now/Next and new-episode badges join the reporter-confirmed subtitle sizing (#12) and Library cover (#13) fixes. New features still need console/provider acceptance. See [release notes](../RELEASE_NOTES.md) and [checks](../TEST_RESULTS.txt).

## Homebrew Store

Published ZIP verification passed with zero errors/warnings, including contentVersion **06.000.005** and the pinned icon. All 175 catalog records validate. Static package scanning passed with zero errors, four maintainer-review warnings and one notice for the existing updater and console-light controls. Helper approvals were not modified.

**Submission blocked:** GitHub denied creating the upstream pull request: “Resource not accessible by integration”. The [validated catalog branch](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/update-nuvio-maxd-1.0.0.5) is pushed and ready.

[Submit the 1.0.0.5 store update](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:update-nuvio-maxd-1.0.0.5?expand=1&title=Update+Nuvio+Max%27d+to+1.0.0.5&body=Updates+Nuvio+Max%27d+to+public+release+1.0.0.5.%0A%0A-+Adds+Library+title+search%2C+provider-supplied+Live+TV+Now%2FNext+%28Xtream%2FXMLTV%29%2C+and+history-aware+new-episode+badges.%0A-+Includes+reporter-confirmed+subtitle+size+and+Trakt+Library+artwork+fixes%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Fissues%2F12+and+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Fissues%2F13.%0A-+Release%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Freleases%2Ftag%2Fv1.0.0.5-native-experimental+%28stable+release%3B+tag+suffix+retained+for+updater+compatibility%29.%0A-+ZIP+and+tagged+param.json+agree+on+contentVersion+06.000.005%3B+icon+pinned+to+release+tag.+Only+apps%2FPPSA99288.json+changes.%0A-+Full+host+regressions%2C+PS5+compilation%2C+deep+image%2FZIP+round+trips+and+source+correspondence+checks.+New+features+still+need+console%2Fprovider+acceptance.%0A-+Static+release+scan%3A+0+errors%2C+4+warnings+and+1+notice+for+the+existing+updater+payload%2Floopback+handoff+and+optional+%2Fdev%2Ficc+console-light+controls.+Helper+approval+was+not+modified%3B+maintainer+review+is+required.%0A%0AZIP+SHA-256%3A+%60f97a8e722e71a6ac257b58674d7f78893ab7a218cbc359892c0fa2edcb2eea7d%60%0A%0AUpdater+helper+SHA-256%3A+%60352525d18e65cf22a18bbd8dd2c4016814e53ea5ebe6a8ed40662603664c7c37%60%0A) — opens the prefilled comparison directly. Choose **Create pull request**, then submit it for maintainer review.

Live store version checked after publication: **1.0.0.4-native-experimental**. Store availability requires maintainer merge and deployment.

## Prepared submission text

Updates Nuvio Max'd to public release 1.0.0.5.

- Adds Library title search, provider-supplied Live TV Now/Next (Xtream/XMLTV), and history-aware new-episode badges.
- Includes reporter-confirmed subtitle size and Trakt Library artwork fixes: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12 and https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13.
- Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.5-native-experimental (stable release; tag suffix retained for updater compatibility).
- ZIP and tagged param.json agree on contentVersion 06.000.005; icon pinned to release tag. Only apps/PPSA99288.json changes.
- Full host regressions, PS5 compilation, deep image/ZIP round trips and source correspondence checks. New features still need console/provider acceptance.
- Static release scan: 0 errors, 4 warnings and 1 notice for the existing updater payload/loopback handoff and optional /dev/icc console-light controls. Helper approval was not modified; maintainer review is required.

ZIP SHA-256: `f97a8e722e71a6ac257b58674d7f78893ab7a218cbc359892c0fa2edcb2eea7d`

Updater helper SHA-256: `352525d18e65cf22a18bbd8dd2c4016814e53ea5ebe6a8ed40662603664c7c37`
