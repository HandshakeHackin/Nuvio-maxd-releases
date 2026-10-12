# Publishing an app update

1. Build and verify the app, ZIP, matching source, checksums, and host CI evidence.
   Copy its `sce_sys/param.json` to the public repository before creating the
   publication tag, so the store reads the same increased `contentVersion` as
   the actual ZIP. Do not retag an already published release to repair metadata.
2. Publish a versioned GitHub release and verify all uploaded asset digests.
3. Submit the same version to ProsperoStore's catalog. Update only
   `apps/PPSA99288.json` in a branch of `HandshakeHackin/ps5-homebrew-catalog`.
   Use the release tag (optionally without its leading `v`), the immutable
   `PPSA99288.zip` URL, GitHub's asset SHA-256 digest, and a reachable icon URL
   pinned to that tag. Experimental prereleases are accepted; draft releases
   and FFPFSC assets are not catalog installation artifacts.
4. Follow the catalog's current submission instructions, run its record checks,
   open or update the catalog PR, and resolve its automated check failures.
   Avoid duplicate submissions when the daily update bot already proposes it.
5. Report both the GitHub release and catalog PR. Confirm store availability
   only after the maintainers merge and the live website/API lists that version.

Do not wait for the catalog's daily discovery/update job instead of submitting
the release. A merge and store deployment remain controlled by the catalog
maintainers. Do not rewrite an older released asset to update a listing.

Catalog: https://github.com/blackbearreloaded/ps5-homebrew-catalog
Submission guide: https://github.com/blackbearreloaded/ps5-homebrew-catalog/blob/main/docs/submitting.md
