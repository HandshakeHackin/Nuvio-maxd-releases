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

The project is GPL-3.0-or-later. Components retain their own notices and license
texts in the source and packaged app. The native UI and player build on
[Sp9nky's unofficial Stremio port](https://github.com/Sp9nky/unofficial-stremio-ps5-port),
[BlackBearReloaded's Prospero UI](https://github.com/blackbearreloaded/ps5-homebrew-ui)
and the other components listed in [THIRD_PARTY.md](native-app/THIRD_PARTY.md).
This is an unofficial Nuvio client, not an official Nuvio or Sony release.
