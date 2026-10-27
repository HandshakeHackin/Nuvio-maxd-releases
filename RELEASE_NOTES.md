# Max’d 1.0.0.2 — IPTV Library and playback updates

- **IPTV Library:** browse your Xtream provider’s movies and shows in a separate sidebar tab. The tab appears only when the active profile has a service offering VOD. Recognized Xtream get.php playlist links can supply the account too; ordinary M3U playlists do not automatically provide a VOD library. Includes provider/category filters, category search, seasons and episodes.
- **Saved catalogs and resume:** keep cached IPTV catalogs during provider outages and resume IPTV titles separately for each profile.
- **Clearer provider errors:** separate guidance for possible provider outages, unavailable titles, rejected access and connection limits. Decoder failures are not labeled as provider outages.
- **Playback updates:** EVO 0.13.0, optimized GPU YUV uploads and Native 10-bit VP9 hardware selection with software fallback when the console refuses it.
- **Xtream categories:** list a live channel in every category assigned by its provider, while keeping one channel identity.
- **Addon/profile polish:** bounded, best-effort addon manifest refresh, search text cleared on profile changes, and improved text layout.
- **Build maintenance:** payload SDK 0.45 with the verified media-library set. Existing HDMI-CEC support and VLC compatibility safeguards are retained.

This follows the import and playback requests in [issue #10](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/10). Large live imports, experimental four-screen TV, PCM boost and audio/subtitle sync from 1.0.0.1 remain included.

## Testing

This is a public test release. Sixteen host verification groups passed, including GPU rendering, EVO decode/seek, VP9 Profile 2 software decode, simulated hardware setup, IPTV discovery/cache/isolation, Xtream imports, tracker contracts, navigation, updater and playback lifecycle. PS5 linking/import checks and deep image/ZIP/source verification passed. These checks do not establish physical-console GPU performance, HDR acceptance or decoder compatibility.

Please try this version and report results in issue #10. See [TESTING.txt](TESTING.txt). For an existing IPTV account, use Live TV’s sources/filters menu → **Check IPTV Library availability** if needed. A live-only provider keeps the VOD tab hidden. VOD network catalogs support up to 100,000 entries / 128 MiB; cache writes have smaller storage bounds. Choose a smaller category for oversized catalogs. Generic M3U VOD inference, DRM, EPG and recording are not added.

For four-screen TV, start with SD channels and a provider allowing four simultaneous connections. VLC’s standalone upscalers/equalizer UI are not embedded; ShadowMount+ remains a separate console update. Native HDR still requires console/output compatibility. Please include the version, player, steps and sanitized playback logs if a problem remains; omit stream URLs, usernames, passwords and tokens.

Package revision: **06.000.002**. Title ID: **PPSA99288**. Close the app before updating and preserve saved user data. Store availability follows catalog approval and deployment.
