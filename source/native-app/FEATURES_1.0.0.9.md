# Nuvio Max’d 1.0.0.9 experimental integration

This is an implementation build based on published 1.0.0.8. Earlier workspaces,
downloads and release assets are preserved. No new GitHub release or store
publication is part of this change.

## Included

- Existing Peer-to-peer torrents switch controls both the engine and a conditional Torrents sidebar page; it remains off by default.
- Magnets, hex/Base32 info hashes, USB/app .torrent files, direct .torrent URLs, file selection, recent history and profile-local resume.
- Read/seek priority, container header/tail retention, bounded rolling cache, webseeds and numerical DHT bootstrap. Two tracker workers, matching the reviewed fork; no eight-worker speed claim.
- Temporary LAN Web Drop on port 8088, started explicitly from its QR screen. Cryptographic pairing token, Host/Origin checks, exact routes, bounded headers/body/queue, ten-minute expiry and console consent. No router mapping for Web Drop. Pending uploads are kept in memory until accepted.
- Auto / Require native HDR10 / Convert HDR to SDR. Auto converts only after SDR restoration succeeds, reuploads the retained picture (including paused playback), and lasts for that session. Existing saved HDR preferences keep their previous behavior. No HDR10+ dynamic-metadata or unsupported Dolby Vision conversion claims.
- Plex/Jellyfin/Emby, issue #14 authenticated artwork/connection persistence, subtitle synchronization, IPTV and previous playback fixes retained.
- No dependency pin changes. Uses the verified 1.0.0.8 SDK/dependency toolchain.

## Validation

Host checks exercise production torrent engine/stream/parser and hub methods with synthetic metadata, seek interruption, file selection, recent/resume state, paired HTTP input, rejected requests, token rotation/expiry, console consent/profile cancellation, opt-out and an in-flight webseed shutdown. Separate HDR checks cover restoration failure, retained-picture conversion and unchanged global preferences. Navigation checks cover all optional-tab combinations. Existing media server, IPTV, playback, GPU and package checks remain required.

A successful PS5 cross-compile and package verification do not establish console runtime compatibility. Before public release, test on a PS5:

1. Enable torrents, open a lawful test magnet and a USB multi-file torrent; seek forward/backward and confirm bounded cache behavior. Disable torrents during discovery and playback, then reopen the app.
2. Pair from a phone on the same trusted LAN; submit a magnet/file, cancel once and accept once. Confirm the listener stops on closing the QR, changing profiles, expiry and disabling torrents. Verify history does not cross profiles.
3. Test SDR, accepted native HDR10 and rejected HDR output in Auto mode; pause before fallback, seek, then open another HDR stream. Require native HDR10 must still report failure, and a refused SDR restoration must never display SDR pixels through HDR scanout.
4. Recheck Plex pairing, Jellyfin/Emby direct/HLS playback, private artwork, saved connections and server progress after a restart.

Fork attribution and its pinned commit are included under app/licenses/torrent-edition.
