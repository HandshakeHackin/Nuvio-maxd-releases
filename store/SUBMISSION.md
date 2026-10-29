# Homebrew Store update — 1.0.0.3

[GitHub release](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.3-native-experimental) is public. All nine uploaded asset sizes and SHA-256 digests match the verified release files.

The catalog record is prepared and pushed, but **the pull request has not been submitted**: GitHub denied `createPullRequest` / `POST /pulls` with “Resource not accessible by integration” (403). The repository owner must open the prepared comparison below and click **Create pull request**, then submit the form.

[Create the Homebrew Store pull request](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:update-nuvio-maxd-1.0.0.3?expand=1&title=Update+Nuvio+Max%27d+to+1.0.0.3&body=Update+Nuvio+Max%E2%80%99d+to+%2A%2A1.0.0.3-native-experimental%2A%2A+%28public+release%29.%0A%0AAdds+A%E2%80%93Z+IPTV+browsing%2C+provider-labelled+search+across+saved+movie%2Fseries+catalogs%2C+embedded+chapter+navigation%2C+audio%2Fsubtitle+reopen+recovery%2C+bounded+artwork+queues+and+conservative+HEVC+colour+metadata+recovery.+Updates+libass%2C+curl%2C+OpenSSL+and+JSON%2C+with+a+startup+module-ordering+adjustment.%0A%0ARelease%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Freleases%2Ftag%2Fv1.0.0.3-native-experimental%0A%0AOnly+%60apps%2FPPSA99288.json%60+changes.+The+ZIP%2C+its+GitHub+asset+SHA-256+and+icon+are+pinned+to+the+release+tag.+Tagged+and+packaged+%60contentVersion%60+is+%6006.000.003%60%2C+increased+from+%6006.000.002%60.%0A%0AValidation+includes+catalog+format+and+published+artifact+verification%2C+native%2FVLC+CI%2C+deep+package%2Fsource+hash+verification+and+static+archive+scanning.+Physical-console%2FHDMI+validation+is+still+pending.%0A%0AThe+static+ZIP+scan+reports+zero+errors+and+four+review+warnings%2C+matching+the+existing+app%E2%80%99s+updater%2Fconsole-light+capabilities%3A%0A%0A-+Bundled+updater+payload+and+its+pending+helper+approval%3A+SHA-256+%60202477186f82b0831c470c9c6ae4b107fe6464ac26d7489d6a82bb526b4b8206%60.%0A-+The+loopback+loader+address+belongs+to+the+app%E2%80%99s+bundled+update-helper+handoff.%0A-+%60%2Fdev%2Ficc_indicator%60+is+used+by+optional+console-light+synchronization.%0A%0AMatching+updater+source+is+under+%60source%2Fnative-app%2Fupdate%2F%60+at+the+release+tag+and+in+its+source+archive.+No+helper+approvals+or+other+app+records+are+changed.+Store+availability+follows+maintainer+review+and+deployment.%0A)

[View the submission branch](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/update-nuvio-maxd-1.0.0.3).

Only `apps/PPSA99288.json` changes. Published artifact verification passed with zero errors and warnings: ZIP SHA-256 `03618aed0b53c48695db23ffeff1dea5ee99f8c8c215f1c9265ef887a83700b2`, contentVersion `06.000.003`. The static archive scan passed with zero errors and four maintainer-review warnings for existing updater/console-light capabilities; no helper approvals were modified.

Homebrew Store availability requires a submitted PR, maintainer approval and deployment. This page does not imply the live store is already updated.

## Prepared submission text

Update Nuvio Max’d to **1.0.0.3-native-experimental** (public release).

Adds A–Z IPTV browsing, provider-labelled search across saved movie/series catalogs, embedded chapter navigation, audio/subtitle reopen recovery, bounded artwork queues and conservative HEVC colour metadata recovery. Updates libass, curl, OpenSSL and JSON, with a startup module-ordering adjustment.

Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.3-native-experimental

Only `apps/PPSA99288.json` changes. The ZIP, its GitHub asset SHA-256 and icon are pinned to the release tag. Tagged and packaged `contentVersion` is `06.000.003`, increased from `06.000.002`.

Validation includes catalog format and published artifact verification, native/VLC CI, deep package/source hash verification and static archive scanning. Physical-console/HDMI validation is still pending.

The static ZIP scan reports zero errors and four review warnings, matching the existing app’s updater/console-light capabilities:

- Bundled updater payload and its pending helper approval: SHA-256 `202477186f82b0831c470c9c6ae4b107fe6464ac26d7489d6a82bb526b4b8206`.
- The loopback loader address belongs to the app’s bundled update-helper handoff.
- `/dev/icc_indicator` is used by optional console-light synchronization.

Matching updater source is under `source/native-app/update/` at the release tag and in its source archive. No helper approvals or other app records are changed. Store availability follows maintainer review and deployment.
