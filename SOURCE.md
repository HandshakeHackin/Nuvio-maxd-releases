# Released source

Each release includes `NuvioMaxd-<version>-source.tar.gz`. This contains the
native app source, build scripts, vendored UI code, dependency pins, license
notices and the public account-service configuration needed to build it.
Extract the archive and read `native-app/README.md` for the toolchain setup.

`source-archive.json` records the development commit and a SHA-256 hash for
every archived source file. The same commit is recorded in the packaged
`build-info.json` and the release validation report. The public tag marks this
download repository's publication commit, so it has a different commit ID.
The development repository stays private; code for released builds is available
without an account through the matching archives here.

The current release's browsable source is on
[`builds/v0.5.31/source/`](https://github.com/HandshakeHackin/Nuvio-maxd-releases/tree/builds/v0.5.31/source).
Its `source-archive.json` identifies the development commit and original hashes.
This main-page documentation update does not change the older source snapshot
under main; use the versioned branch or matching release archive to build 0.5.31.
GitHub's language statistics exclude vendored libraries and release-only scripts.

The project is GPL-3.0-or-later. Components retain their own notices and license
texts in the source and packaged app. The native UI and player build on
[Sp9nky's unofficial Stremio port](https://github.com/Sp9nky/unofficial-stremio-ps5-port),
[BlackBearReloaded's Prospero UI](https://github.com/blackbearreloaded/ps5-homebrew-ui)
and the other components listed in [THIRD_PARTY.md](source/native-app/THIRD_PARTY.md).
This is an unofficial Nuvio client, not an official Nuvio or Sony release.
