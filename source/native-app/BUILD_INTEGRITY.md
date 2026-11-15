# Offline release-file checks

This build verifies signed release metadata, then SHA-256 hashes for the named
package files, including `CREDITS.txt`, `LICENSE`, `THIRD_PARTY.md`, the executable,
branding metadata and build information. A mismatch or failed verification shows
one dismissible **Unverified build** notice at the next idle screen. Playback,
sign-in, profile selection and user data are not blocked or changed.

**Settings → App & Updates → Build authenticity & credits** provides the project
link, attribution, license information and a manual recheck. The text is available
in all eight menu languages. Credits are compiled into the interface as well as
shipped in the signed package, so missing credit files do not erase the project link.

Checks run in an app-scoped background worker, independently of profile changes.
They stop on shutdown and are limited to a 128 KiB manifest, 512 files, 128 MiB per
file, 256 MiB total and a 30-second budget checked between local reads. Authentication
precedes manifest parsing and file access. Paths must be bounded, unique, ordered
and relative; traversal, duplicate entries and missing required notices are rejected.

Only named package files under the app root are hashed. Save/profile/cache folders
are not scanned. There are no device or account identifiers, installation beacons,
network requests or reporting. Verification does not enumerate unexpected extra
files, prevent filesystem races, identify who copied the app, or act as a security
sandbox. Identical copies verify identically. Modified source can change/remove
the checks. GPL permissions and third-party licenses remain unchanged.

## Signing builds

The tracked `release-public-key.pem` and `src/release_public_key.hpp` contain the
public verification key only. The official Ed25519 private key stays outside Git,
the package and source archives, with owner-only file permissions. Back up that
private key through an owner-controlled channel; it cannot be recovered from the
public key or a published app. Do not share it with fork builders.

For an owner-signed build, set `NUVIO_RELEASE_SIGNING_KEY` to that external PEM
file when running `gl/build.sh`. The normal app build copies all files, writes
build information and then creates `release-manifest.json` and its Ed25519
`release-manifest.sig`. The producer requires clean committed source, the matching
public key and complete credits before signing. It rejects private PEM keys in
the package. Signing includes the executable hash without embedding the manifest
in the executable, avoiding a self-hash cycle.

Without the private key, ordinary development/source-archive builds remain
buildable and unsigned; the informational warning is expected. Fork maintainers
can use their own branding and verification key in modified source while meeting
the applicable license and source obligations. No official signing key is needed
to run or modify the app.

`tests/build_integrity.sh` exercises real Ed25519 and SHA-256, branding/credit
changes, signature/manifest tampering, malformed paths/metadata, size/count budgets,
immediate cancellation/deadline, private-key exclusion and unsigned builds. It also
compiles production Session scheduling to test profile-independent results,
modal/playback deferral, duplicate suppression, manual refresh and shutdown safety.
