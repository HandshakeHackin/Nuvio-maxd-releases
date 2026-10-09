# ProsperoStore submission

Nuvio Max'd has been submitted in [catalog pull request #109](https://github.com/blackbearreloaded/ps5-homebrew-catalog/pull/109). It is awaiting maintainer review and is not yet an accepted store listing.

Both [automated checks](https://github.com/blackbearreloaded/ps5-homebrew-catalog/actions/runs/37969844369) passed:

- **Validate submission:** no errors or warnings. The publisher, record, public release, exact ZIP checksum, pinned icon and content version `05.000.009` were verified.
- **Scan the release:** no errors and three warnings concerning the bundled updater payload, its local loader handoff and its digest not yet being on the approved helper list. The maintainer needs to review the helper source before accepting it.

The [submission branch](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/submit-nuvio-maxd) adds only `apps/PPSA99288.json`. Matching source and license notices are attached to the public release. The [submission description](SUBMISSION.md) discloses the experimental features, credited upstream components and bundled helper.

Store maintainers make the approval decision. Once they merge the listing, the catalog and storefront can publish it. Downloads remain available from [GitHub Releases](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases).

See [the official submission guide](https://github.com/blackbearreloaded/ps5-homebrew-catalog/blob/main/docs/submitting.md).
