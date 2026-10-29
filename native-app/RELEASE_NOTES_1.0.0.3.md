# Max’d 1.0.0.3

## IPTV Library

- Added A–Z navigation for movie and series catalogs, with common accented letters grouped under their base letter and numbers/other scripts under #.
- Added search across saved movie and series catalogs from the active profile’s imported providers. Results identify the provider and content type; playback and episode lookup use the selected provider.
- Kept search offline-capable, bounded and cancellable, with duplicate removal and protection against stale replies after switching profiles or providers.
- Kept long provider badges inside their poster cards. IPTV titles and resume progress remain separate from addon catalogs and tracker history.

## Playback

- Added embedded chapter navigation and timeline markers for supported Native, EVO and VLC files. Open Options/Up, select the Chapters column, then press Cross to jump. The column appears only when at least two valid chapters exist.
- Improved recovery of the selected audio/subtitle track, pause state, audio sync and PCM gain after playback reopens. Explicit subtitles Off and downloaded external cues are preserved where supported.
- Added a bounded HEVC SPS/VUI fallback for missing colour metadata, while preserving explicit container tags. This does not add Dolby Vision output or change whether the console accepts HDR10 output.
- Limited artwork queues, decoded-image staging and cache metadata to reduce memory pressure during browsing and release resources on shutdown.

## Maintenance

- Updated libass to 0.17.5, curl to 8.23.0, OpenSSL to 3.5.9 and nlohmann/json to 3.12.0.
- Adjusted startup ordering to preload VideoDec2 before graphics initialization; decoder queries remain deferred until playback.
- Added checksum-verified dependency builds and refreshed VLC’s dependency links so older libraries cannot silently remain in the build.
- Updated the in-app changelog for this release. Existing automatic update reminders remain available under Settings → App updates.

Saved IPTV search covers catalogs already cached for the active profile; it does not download every provider catalog or search the Live TV guide. Open other categories or All categories to save more titles. Episodes keep their normal season order.

Host regressions and PS5 compile/link/import checks passed. Physical-console playback, old-firmware startup, HDMI/HDR and receiver behavior still need testing. Existing optional/experimental player, HDR and four-screen features retain their limitations.

Package revision: **06.000.003**. Title ID: **PPSA99288**. Close the app before updating and preserve saved user data. Homebrew Store availability follows catalog approval and deployment.
