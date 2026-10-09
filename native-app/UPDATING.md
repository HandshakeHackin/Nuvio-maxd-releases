# Updates without a separate ELF download

Nuvio Max'd includes **Settings → App updates**, introduced in 0.5.7. It checks a public GitHub download
feed, shows the available version and downloads its FFPFSC. Your Nuvio account
isn't used for this request, and you don't need a GitHub login.

Choose **Download update**, then **Install and close Nuvio Max'd** after verification.
The app closes normally, saving Watch Progress and restoring the display. Its
bundled helper handles the replacement and sends a PS5 notification. Reopen
Nuvio Max'd after ShadowMount+ finishes its rescan.

The installer requires ShadowMount+ **1.7 with public HTTP API v1 enabled on
loopback port 10101**, and a payload loader on **loopback port 9021**. Nuvio Max'd
must be installed as an FFPFSC under a managed `/homebrew/` folder on internal,
USB or extended storage. Folder installs and PKGs use manual updates.

The helper is included in the FFPFSC. You never download or launch an ELF
yourself. It runs outside the app sandbox so it can reach the original image
after Nuvio Max'd exits. The app uses ShadowMount's documented HTTP API; it does not
use the private control socket or change ShadowMount settings.

## What happens to your existing installation

The download must match GitHub's SHA-256 digest and advertised size. Before
closing, the helper verifies a second copy staged beside the installed image.
After closing, it requests a guarded unmount and checks the image snapshot.
It copies and verifies the old image as a backup, then commits the replacement
with one rename on the same filesystem. There is no step that deletes the
installed image before the replacement is ready.

The backup has a `.nuvio-backup-….bak` suffix so it isn't scanned as a second
FFPFSC. Keep it until the new version works. For a manual rollback, close
Nuvio Max'd, release its mount and restore that backup to the original `.ffpfsc`
path. Your account, settings and progress in `/download0/nuvio` are untouched.

Cancelling, a bad hash, disconnected storage, a changed install or a failed
backup leaves the original image installed. A successful scan request only
queues a rescan; it isn't confirmation that ShadowMount has finished.

## Current limits

This installer is experimental. Its file transaction and failure paths have
host tests, and the app and helper build with the PS5 SDK. The loader handoff,
exFAT durability and unmount/rescan sequence still need console testing.
ShadowMount doesn't provide an exclusive third-party update transaction;
avoid concurrent mount or storage operations while installing.

With `persistent_image_mounts=1`, ShadowMount can keep the backing image mounted
after releasing the app. Nuvio Max'd refuses to replace that image. It leaves your
installation intact and shows a notification; use manual replacement once the
mount is released. It doesn't silently change your ShadowMount configuration.

Only newer `vX.Y.Z-native-experimental` releases with the expected native image,
GitHub asset digest and matching project source archive are offered. A stable
release channel can be added after console validation. The feed is checked
when you ask; updates are never installed automatically in the background.

## Public feed

The configured feed is `HandshakeHackin/Nuvio-maxd-releases`. The source GitHub
repository remains private. Public releases include the ZIP, FFPFSC, validation
report, SHA256SUMS and the matching released source archive, including its
license notices, dependency pins and build instructions. Released code is
therefore public even though the development repository stays private.

Version 0.5.9 is the first Max'd build pointed at this feed. Earlier builds use
the former feed address and need one manual update. ZIP folder installs can be
managed by ProsperoStore once listed, or updated by replacing the app folder.
The bundled installer handles FFPFSC images only.
