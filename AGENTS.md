# Release requirements

The owner requires every new GitHub app release to include a corresponding
ProsperoStore/Homebrew Store catalog update submission in the same release task.
This authorization includes pushing the catalog branch and opening its pull
request; do not ask again for routine release submissions.

Follow RELEASING.md. Submit only apps/PPSA99288.json to
darkxex/ps5-homebrew-catalog from HandshakeHackin's fork. Use the
published PPSA99288.zip asset's GitHub SHA-256 digest and version tag. Preserve
existing release assets and tags. Check for an existing proposal before opening
a duplicate; resolve submission check failures within the authorized scope.

Report the catalog PR link and status. A submitted PR is not a live store
update: catalog maintainers must merge it and deploy the site/API. Never claim
the store has updated without checking its live catalog.

Use `builds/v<VERSION>` for versioned delivery branches, including experimental
builds. Preserve earlier build branches and published release assets.
