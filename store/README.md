# ProsperoStore listing

[1.0.0 is published on GitHub](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0-native-experimental).

**The store update is prepared, but its pull request has not been submitted.** GitHub returned `403 Resource not accessible by integration` when both the connected integration and CLI tried to create the upstream pull request. The verified update branch is pushed to the owner’s fork.

[**Submit the 1.0.0 store update — open the prefilled pull request**](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:update-nuvio-maxd-1.0.0?expand=1&title=Update+Nuvio+Max%E2%80%99d+to+1.0.0&body=Update+Nuvio+Max%27d+to+1.0.0.%0A%0AAdds+M3U%2FXtream+Live+TV%2C+trailers%2C+anime+timing+providers%2C+settings+search%2C+source+caching+and+playback+badges.+Includes+audio-track%2C+seeking%2C+Trakt+pairing%2C+provider+and+updater+fixes.%0A%0AThe+record+points+to+the+immutable+published+GitHub+ZIP+and+its+verified+asset+SHA-256.+Tagged+and+packaged+contentVersion+is+%6006.000.000%60.+Only+%60apps%2FPPSA99288.json%60+changes.%0A%0AValidation%3A+offline+catalog+checks%2C+unit+suite+and+release+ZIP+scan.+The+scan+has+no+errors+and+flags+the+bundled+update+helper+for+maintainer+review%3B+this+PR+does+not+change+helper+approvals.+Console+acceptance+of+all+new+features+remains+pending.%0A). Review the one-file diff, then select **Create pull request** on GitHub. This link opens the compare/submission screen directly.

Validation passed: offline catalog checks; 205 catalog unit tests (two skipped); online release, ZIP digest and tagged metadata checks; and the ZIP scan with zero errors, two warnings and one notice. The bundled update helper requires catalog maintainer review; helper approval was not changed. Tagged and packaged contentVersion is `06.000.000`.

After submission, catalog maintainers must merge and deploy before the [live store listing](https://prosperostore.net/app/PPSA99288/) changes. GitHub publication does not itself update the store.

See [release notes](../RELEASE_NOTES.md) and [console testing](../native-app/TESTING-1.0.0.md).
