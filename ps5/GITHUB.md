# GitHub releases

Public downloads live in
[HandshakeHackin/Nuvio-maxd-releases](https://github.com/HandshakeHackin/Nuvio-maxd-releases).
The development repository, `HandshakeHackin/nuvio-ps5`, remains private.
Each public release includes its matching native source archive and build guide.

For installation and Watch Progress setup, use the [main README](../README.md).
For compiling the app, use the [native build guide](../native-app/README.md).
The earlier 0.3 companion PKGs are archived; current releases provide a ZIP app folder and an FFPFSC image.

## Preparing a native release

Commit the source before the final build so `build-info.json` records a clean
revision. Run the host checks and keep their complete log:

```sh
bash native-app/tests/run.sh > /tmp/nuvio-host-tests.log 2>&1
```

Build the image using the native guide. For v0.5.9, copy it into a release
folder under the download filename. Copy the ZIP too. Verify the image against the staged app, then verify every
ZIP file against the extracted image:

```sh
mkdir -p /tmp/nuvio-release
cp native-app/dist/PPSA99288.ffpfsc /tmp/nuvio-release/NuvioMaxd-0.5.9-native.ffpfsc
cp native-app/dist/PPSA99288.zip /tmp/nuvio-release/PPSA99288.zip
python3 native-app/tools/verify_image.py \
  /tmp/nuvio-release/NuvioMaxd-0.5.9-native.ffpfsc \
  native-app/dist/PPSA99288 \
  /path/to/ps5-native-app-boilerplate/.deps/MkPFS/.mkpfs-run-linux \
  /tmp/nuvio-release/native-validation.json \
  --host-test-log /tmp/nuvio-host-tests.log
python3 native-app/tools/verify_zip.py \
  /tmp/nuvio-release/PPSA99288.zip /tmp/nuvio-release/native-validation.json
```

Create the matching source archive, then generate checksums after verification:

```sh
python3 native-app/tools/source_archive.py HEAD /tmp/nuvio-release/NuvioMaxd-0.5.9-source.tar.gz
cd /tmp/nuvio-release
sha256sum PPSA99288.zip NuvioMaxd-0.5.9-native.ffpfsc native-validation.json NuvioMaxd-0.5.9-source.tar.gz > SHA256SUMS
```

Upload these five assets:

- `PPSA99288.zip`
- `NuvioMaxd-0.5.9-native.ffpfsc`
- `NuvioMaxd-0.5.9-source.tar.gz`
- `native-validation.json`
- `SHA256SUMS`, covering the ZIP, image, validation report and source

Keep compiled binaries, SDKs and personal configuration out of the source
branch. Account tokens and playable URLs shouldn't appear in release notes,
logs or issues.

## Checks and release notes

The **Native Nuvio Max'd checks** workflow runs the current native host suite.
**PS5 host checks** covers the retained browser/bridge experiment. Passing the
host workflows doesn't establish console or firmware compatibility.

Publish it as a prerelease while console testing is still pending. The current tag is `v0.5.9-native-experimental`.
Describe what changed, how to update, any known limits and what needs retesting.
Keep older releases available so a console tester can go back to a working build.

After uploading, compare each asset's size and SHA-256 digest with the local
validated files. `sourceCommit` in the report and source archive identifies the private
development revision used to build the app. The public release tag identifies
the download repository's publication revision; their commit IDs differ. A documentation-only update can land on `main` without replacing an
already validated app image.

## Public download feed

The public feed is `HandshakeHackin/Nuvio-maxd-releases`. Keep the GPL license
and the current `sce_sys/icon0.png` and `sce_sys/param.json` in its default branch
and at the release tag for store discovery.

`native-app/release-feed/` contains the feed README, a publish workflow and a
validator. Copy the workflow to `.github/workflows/publish.yml`, the validator
to the repository root and the README to `README.md`. Stage the five verified
release assets under `release-payload/` and the notes in `release-notes.md` on
a temporary `publish/v0.5.9-native-experimental` branch. Its workflow checks
all hashes and matching source metadata, then publishes a prerelease. Remove
that temporary branch after checking the uploaded assets.

The ZIP is the ProsperoStore submission format. A public release and a catalog
record prepare the app for review; they do not mean it is accepted or listed.

Suggested About description: **Nuvio Max'd is an unofficial native PS5 client
with synced watch progress, addon streams, episode skipping and experimental
HDR10 playback. Public downloads include matching source.**
