# 1.0.0.1 catalog submission

Status: validated and pushed; PR creation blocked by GitHub integration permissions. Use the [prefilled submission link](README.md).

Update Nuvio Max'd to 1.0.0.1-native-experimental.

Fixes large M3U/Xtream live-channel imports and adds experimental four-screen Live TV, PCM volume/boost and audio/subtitle sync controls. Four-screen mode needs four provider connections; SD feeds are recommended, and physical-console confirmation remains pending.

Release: https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.1-native-experimental

Only `apps/PPSA99288.json` changes. The ZIP and icon are pinned to the new release tag. Tagged and packaged `contentVersion` is `06.000.001`, increased from `06.000.000`. The record's SHA-256 matches the published GitHub asset digest.

Validation: catalog format and online checks; static ZIP scan (zero errors). The scan flags the bundled update helper (`228293532c67d98b1a8aca99071cc9db928a1f548e1087b35e4843aa829c7572`) for maintainer review, as with the previous release. Matching source is included in the release and browsable under `source/native-app/update/` at its tag. This PR does not alter helper approvals. Native/VLC host CI and package verification evidence accompany the release.
