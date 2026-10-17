# ProsperoStore update status

GitHub release: [0.5.30-native-experimental](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v0.5.30-native-experimental).

The verified ZIP uses contentVersion **05.000.030** and SHA-256
`32d7d4480a7ef3cfb88f728513c9d50522afb98363ee05fef17b8c064c55c34b`.
Online catalog verification passes with no errors or warnings. The full static
scan has 0 errors, 4 warnings and 1 notice; the updater helper requires maintainer
review. See the attached release scan report.

The update is pushed to `HandshakeHackin/ps5-homebrew-catalog`, branch
`update-nuvio-maxd-0.5.30`, commit `1a71218`. Only `apps/PPSA99288.json` changes.
Both CLI and connector attempts to create the upstream PR were rejected with
HTTP 403, “Resource not accessible by integration”. **No 0.5.30 catalog PR has
been submitted by this session.**

[Open the prefilled catalog update request](https://github.com/blackbearreloaded/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:update-nuvio-maxd-0.5.30?expand=1&title=Update+Nuvio+Max%27d+to+0.5.30&body=Update+PPSA99288+to+0.5.30-native-experimental+%28contentVersion+05.000.030%29.%0A%0ARelease+and+full+validation%2Fscan+evidence%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2FNuvio-maxd-releases%2Freleases%2Ftag%2Fv0.5.30-native-experimental%0A%0AAdds+account+profiles%2C+fixes+provider-menu+scrolling%2C+and+includes+a+possible+provider-download+fix.+Related+issues+%233+and+%234+are+linked+in+the+release+changelog.+PS5+and+real-account+testing+remain+required.%0A%0AOnly+apps%2FPPSA99288.json+changes.+Online+catalog+verification+passes.+Static+scan%3A+0+errors%2C+4+warnings%2C+1+notice%3B+the+bundled+updater+requires+maintainer+helper+review.+Full+details+are+attached+to+the+release.+Assets+and+tags+will+not+be+replaced.%0A). Click **Create pull request** while signed in as HandshakeHackin.

The live store still lists **0.5.29-native-experimental** when checked on
2026-10-17. Submission, maintainer merge and deployment are separate; the store
is not yet confirmed updated to 0.5.30.

[Changelog and linked reports](../CHANGELOG.md) · [Store listing](https://homebrew.page/app/PPSA99288/)
