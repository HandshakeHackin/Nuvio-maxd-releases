# Nuvio Max'd 1.0.0.11

## Security and maintenance
- Replace the legacy provider HTML-selector fork with maintained Cheerio 1.2.0 slim. This removes the known nth-check denial-of-service advisory while preserving existing provider module names.
- Update subtitle shaping to HarfBuzz 15.1.0, artwork handling to WebP 1.6.0 and XML parsing to libxml2 2.15.4.
- Use the PS5 SDK's secure random API for XML hash randomization. Keep the verified FFmpeg 7.0.3 archive pins and existing Native/EVO/VLC playback integration.
- Add weekly dependency/security monitoring and reviewable update proposals. No automatic app upgrades or publishing.

## Included from recent test builds
- Optional on-console torrent and magnet streaming with sequential buffering, plus paired phone/PC Web Drop. Torrent streaming is off by default and requires console confirmation.
- Automatic HDR-to-SDR fallback when native HDR output is rejected. Strict native HDR and explicit SDR conversion remain available.
- Experimental app-local Media Remote/CEC button testing and remapping under Settings > Controls > Remote buttons. PlayStation or the TV can intercept system buttons; Nuvio cannot remap presses it never receives. DualSense controls are unchanged.
- Plex, Jellyfin and Emby browsing/playback, authenticated artwork and saved connection fixes remain included, along with profiles, Live TV/IPTV and watch progress.

## Installation and verification
Use PPSA99288.zip for a folder install or NuvioMaxd-1.0.0.11-native.ffpfsc for an image install. Preserve saved user data and keep the previous installer. Content version: 07.000.011.

The full PS5 app and updater compile/link, host regression tests, updated-library fixtures, matching source and package integrity are verified. npm reports zero known advisories for the pinned JavaScript graph at the time of verification; this is not a blanket native-library security clearance.

Physical PS5 acceptance of the updated libraries remains pending. Torrent networking, remote buttons, HDR output and optional EVO/VLC playback retain their documented experimental limits. EVO remains the integrated 0.13.0 decoder with our existing HEVC base-layer fix; this release does not add full Dolby Vision output.
