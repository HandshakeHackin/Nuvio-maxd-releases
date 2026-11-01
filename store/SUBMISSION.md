# 1.0.0.4 test release delivery

[Download the test prerelease](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.4-native-experimental). All nine uploaded asset digests and sizes match the verified local artifacts. Stable GitHub release remains 1.0.0.3.

Reporter invitations are posted:

- [Issue #12 — subtitle sizing](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12#issuecomment-6559485670)
- [Issue #13 — missing Library covers](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13#issuecomment-6559486322)

Both issues remain open pending console confirmation.

## Catalog review status

The one-record update is [prepared and pushed](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/update-nuvio-maxd-1.0.0.4). Published artifact verification passed with zero errors/warnings; contentVersion is 06.000.004. Static scanning passed with zero errors and four review warnings for existing updater/console-light capabilities. No helper approvals were changed.

**No upstream PR was created:** GitHub denied createPullRequest with “Resource not accessible by integration”. This is a targeted test build; do not deploy it to the store before reporter confirmation. When review is appropriate, the repository owner can use the prepared comparison below and choose **Create draft pull request**. Maintainer review and deployment are separate steps.

[Prepared catalog comparison](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:update-nuvio-maxd-1.0.0.4?expand=1&title=Draft%3A+Nuvio+Max%27d+1.0.0.4+issue-test+build+%E2%80%94+await+reporter+confirmation&body=Prepare+Nuvio+Max%E2%80%99d+%2A%2A1.0.0.4-native-experimental%2A%2A+for+catalog+review.%0A%0A%2A%2ATargeted+prerelease+for+issues+%2312+and+%2313.+Please+keep+this+PR+in+draft+%2F+do+not+merge+or+deploy+until+the+reporters+confirm+the+fixes.+Stable+GitHub+release+remains+1.0.0.3.%2A%2A%0A%0A-+Fix+plain+text%2Flibass+subtitle+sizes+to+match+the+preview%3B+add+150%25%2F200%25+options.%0A-+Load+missing+Library+posters+independently+of+the+Board%2C+using+bounded+metadata+requests+and+profile+isolation.%0A%0ARelease%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Freleases%2Ftag%2Fv1.0.0.4-native-experimental%0AReports%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Fissues%2F12+and+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Fissues%2F13%0A%0AOnly+apps%2FPPSA99288.json+changes.+ZIP+and+icon+URLs+are+pinned+to+the+release+tag.+Packaged+and+tagged+contentVersion%3A+06.000.004.+Tests+and+verified+checksums+accompany+the+release%3B+physical-console+acceptance+remains+pending.%0A%0AStatic+scan+has+zero+errors+and+four+review+warnings+for+existing+updater%2Fconsole-light+capabilities%2C+including+the+bundled+update+helper+and+loopback+loader+handoff.+Matching+helper+source+is+in+source%2Fnative-app%2Fupdate%2F+at+the+release+tag+and+in+the+source+archive.+No+helper+approvals+or+other+app+records+are+modified.%0A%0AZIP+SHA-256%3A+%602383acc24839fc66b87b655f602b13277b372e9dbacdec18affbfcdebff20510%60.+Updater+helper+SHA-256%3A+%60fe9c10d45b412b172999177551c589add81699e2c09f9b7d9ce260c7e54b81c9%60.%0A)

## Prepared draft text

Prepare Nuvio Max’d **1.0.0.4-native-experimental** for catalog review.

**Targeted prerelease for issues #12 and #13. Please keep this PR in draft / do not merge or deploy until the reporters confirm the fixes. Stable GitHub release remains 1.0.0.3.**

- Fix plain text/libass subtitle sizes to match the preview; add 150%/200% options.
- Load missing Library posters independently of the Board, using bounded metadata requests and profile isolation.

Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.4-native-experimental
Reports: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12 and https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13

Only apps/PPSA99288.json changes. ZIP and icon URLs are pinned to the release tag. Packaged and tagged contentVersion: 06.000.004. Tests and verified checksums accompany the release; physical-console acceptance remains pending.

Static scan has zero errors and four review warnings for existing updater/console-light capabilities, including the bundled update helper and loopback loader handoff. Matching helper source is in source/native-app/update/ at the release tag and in the source archive. No helper approvals or other app records are modified.

ZIP SHA-256: `2383acc24839fc66b87b655f602b13277b372e9dbacdec18affbfcdebff20510`. Updater helper SHA-256: `fe9c10d45b412b172999177551c589add81699e2c09f9b7d9ce260c7e54b81c9`.
