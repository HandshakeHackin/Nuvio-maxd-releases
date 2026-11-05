# Nuvio Max'd 1.0.0.8 experimental

- Fix authenticated Media Servers artwork and verify saved connections after writing. Connections and private caches stay separate for each profile.
- Include the issue #14 password-keyboard fix with masked Basic Latin input. The reporter confirmed that keyboard fix on firmware 7.00; connection persistence after a real console restart still needs confirmation.
- Add experimental Plex, Jellyfin and Emby browsing, search, playback and server watch progress under Settings → Account & Services → Media servers.
- Include dependency maintenance, Native/EVO HEVC base-layer filtering, PCM Night Mode, guide clock correction, chapter controls and optional subtitle auto-sync.

Replace the existing PPSA99288 app folder with the ZIP contents, or replace the old FFPFSC image. Keep user data to preserve profiles and saved connections. Use one install format at a time.

This is an experimental prerelease. Host and package verification do not establish PS5 firmware compatibility, real server persistence, HDR output or live service interoperability. Test on console and retain the previous installer for rollback. Do not include account tokens or playable URLs in bug reports.
