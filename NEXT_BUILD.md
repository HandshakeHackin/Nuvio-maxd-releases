# Next build: 0.5.32 — MAX'D family icon

Staged app source: `69ac63b84b0a6480c90317ea6d01acb98e570925`, source
repository branch `builds/v0.5.32`. This public branch stages matching source,
package metadata and GitHub artwork. No 0.5.32 binary or tag is published yet.

- PS5 icon: `source/native-app/app/sce_sys/icon0.png` (512 × 512 RGB PNG).
- GitHub/store icon: `sce_sys/icon0.png` (identical bytes).
- Master: `branding/maxd-nuvio-master.png`.
- Frontend icon: `assets/images/icon.png` in the source repository.
- Content version: `05.000.032`; app/update metadata: `0.5.32`.
- In-app change notes include the MAX'D family icon.

For rollout, build and verify the next package/source against its final source
commit. Confirm the image, ZIP and public tagged `sce_sys/icon0.png` all contain
this same icon, then publish a new version/tag. Update the main-page icon,
download links and changelog together. Pin the store icon/ZIP to that new tag,
use the published ZIP digest and submit the store catalog update to the current
`darkxex/ps5-homebrew-catalog` repository. Verify deployment at
https://prosperostore.net/app/PPSA99288/ before claiming store availability.

Existing 0.5.31 tags/assets stay immutable. The staged store record still points
to 0.5.31 until the next verified ZIP/digest exists; do not submit that record as
0.5.32 before the package is published.

## Updater shutdown candidate fix — issue #7

A fresh-launch shutdown could dereference the null episode-task cancellation
flag before sending the updater's exit handoff. The optional flag is now guarded.
The exact production shutdown body crashes in the boundary fixture before the
fix and passes afterward; all 28 updater transaction tests also pass. The actual
session source passed a compiler check. PS5 loader/mount/UI acceptance and the
reporter's exact failure still require confirmation.

See [the investigation](source/native-app/UPDATER_INVESTIGATION_0.5.32.md).
Install the fixed version through the store or manually once when it is released;
a downloaded image cannot repair the old updater that is currently running.
