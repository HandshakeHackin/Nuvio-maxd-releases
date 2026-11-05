# Nuvio Max'd 1.0.0.9 experimental

## Torrent streaming
- Enable Settings > Addons > Peer-to-peer torrents to reveal the torrent hub. Open magnets, info hashes, USB/app .torrent files and direct .torrent URLs; choose a file and resume recent streams separately for each profile.
- Improve read/seek buffering, container header/tail retention and numerical DHT bootstrap. Use a configurable 1, 2, 4 or 6 GiB rolling cache and two tracker workers.
- Pair a phone or PC using the Web Drop QR screen on the same trusted LAN. The temporary listener uses port 8088, expires after ten minutes and requires console confirmation before opening a received torrent. Closing the QR, changing profiles or disabling torrents stops it.

## HDR and existing features
- Add Auto HDR-to-SDR fallback after successful SDR output restoration. The fallback affects only the current playback session; existing saved HDR choices retain their behavior. Require native HDR10 and Convert HDR to SDR remain available.
- Preserve Plex, Jellyfin, Emby, issue #14 authenticated artwork/connection persistence, IPTV and previous subtitle/playback fixes.
- Fix torrent worker shutdown and reject malformed metadata lengths before arithmetic or allocation. Adapted from chewlord76/nuvio-torrent-edition with attribution included.

## Installation and testing
Use PPSA99288.zip for ProsperoStore/folder installation, or the FFPFSC image for image installation. Keep user data and use one installation format at a time. Content version: 07.000.009.

Experimental prerelease: automated host, sanitizer, UI and package verification passed; PS5 runtime, real torrent swarms and display/server compatibility still need console testing. HDR10+ dynamic metadata and unsupported Dolby Vision conversion are not claimed.
