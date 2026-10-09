# ProsperoStore listing

The public GitHub release is now **0.5.14-native-experimental**. The local
`PPSA99288.json` describes that verified release; the live catalog still pins
0.5.9 until a catalog update is accepted.

Nuvio Max'd is [available in ProsperoStore](https://homebrew.page/app/PPSA99288/). Store maintainers accepted and merged [catalog pull request #109](https://github.com/blackbearreloaded/ps5-homebrew-catalog/pull/109).

The live catalog lists version **0.5.9-native-experimental**, title ID **PPSA99288** and content version **05.000.009**, with the published `PPSA99288.zip` as its download. Find **Nuvio Max'd** in the console store to install it. ZIP and FFPFSC downloads also remain available from [GitHub Releases](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases).

Both [submission checks](https://github.com/blackbearreloaded/ps5-homebrew-catalog/actions/runs/37969844369) passed. The release scan reported three warnings concerning the bundled updater payload, its local loader handoff and the helper digest not being on the approved list at submission time. These findings were disclosed in the [submission description](SUBMISSION.md), along with the helper source, credited upstream components and experimental features.

The catalog pins the release ZIP's checksum. Future releases should use new tags and assets rather than replacing the listed download. Matching source and license notices are attached to every public release.

See [the official submission and update guide](https://github.com/blackbearreloaded/ps5-homebrew-catalog/blob/main/docs/submitting.md).
