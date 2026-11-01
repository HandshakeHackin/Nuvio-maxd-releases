# Max’d 1.0.0.5

- **Search your Library:** Press Triangle/Options or choose Search to find titles already loaded from the active profile’s selected Library source. Search works alongside type and sorting filters. Clear Search or Circle restores the list.
- **Live TV Now/Next:** Channel cards show current and upcoming programmes when the provider supplies guide data. Supports Xtream short EPG, XMLTV links in M3U playlists and an optional XMLTV URL. Live TV → Options includes guide URL, refresh and on/off controls.
- **New-episode badges:** Series in your Library can show labels such as “New: S2 E3” when synchronized watched history and dated episode metadata identify an aired, unwatched update. Future, unknown, special and in-progress episodes are excluded.
- **Larger external subtitles:** Playback and preview share their size scale, with Extra large (150%) and Largest (200%) options. Existing preferences are retained. [Reporter confirmed #12](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12#issuecomment-6560822122).
- **Missing Library covers fixed:** Library titles request their own missing metadata instead of depending on Board browsing. [Reporter confirmed #13](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13#issuecomment-6560101332).

Search and guide/episode caches stay within the active profile/source. Guide requests run in the background; XMLTV documents are bounded and reused across channel pages. Missing guide data leaves channels playable. Episode metadata requests are bounded and do not change watched marks or Library membership.

Library search covers loaded titles; normal Library synchronization continues independently. Guides require matching provider channel IDs and programme data. Episode badges require reliable history and release dates; date-only releases wait until the UTC day has ended. Authored ASS and bitmap subtitles retain their existing behavior.

Automated provider/session, navigation and rendering regressions and PS5 compile/link/import checks passed. The two reported fixes are console-confirmed; the new features still need physical-console/provider acceptance. Existing optional player, HDR and four-screen limitations remain.

Title ID: **PPSA99288**. Package contentVersion: **06.000.005**. Close the app before updating and preserve saved user data. Homebrew Store availability follows catalog approval and deployment.
