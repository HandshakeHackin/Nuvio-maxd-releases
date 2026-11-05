# Nuvio Max'd 1.0.0.11 — console checklist

This is an official release. Updated libraries and experimental features still need physical PS5 acceptance. Keep saved user data, a controller and your previous installer. Use content version 07.000.011 from the matching ZIP folder or FFPFSC image.

1. Open the app, sign in/select a profile, browse Home and Library, scroll through cold artwork and reopen after closing the app.
2. Enable an existing provider plugin. Check HTML-based search/results and stream selection; note the provider name/version if parsing differs. Do not share account tokens or private stream URLs.
3. Play SDR H.264 and HEVC files. Test pause, seeking, audio/subtitle selection and resume. Repeat with Native, EVO and VLC as appropriate.
4. Test text subtitles in Latin, Hebrew, Arabic, Thai and CJK; verify shaping, size and rendering. Styled ASS and bitmap/PGS behavior should remain intact.
5. Connect Jellyfin/Emby/Plex. Check authenticated artwork, movies/episodes, direct/HLS playback and progress. Close/reopen, then restart the console to check saved connections.
6. Optional torrent test: explicitly enable torrent streaming, accept the console confirmation, use a lawful test torrent/magnet and check buffering, seeking, stop and cancellation. Disable it afterwards if unused. Test paired Web Drop from the local network.
7. On a compatible display, test Auto HDR with HDR10 and an output rejection if available. Confirm SDR fallback remains playable; explicit SDR/strict HDR preferences should persist.
8. Optional remote test: Settings > Controls > Remote buttons > Test remote buttons. Only assign buttons Nuvio detects. Check assignment, change/remove/reset, close/reopen persistence and overlay return. The app cannot change OS-level or TV-intercepted buttons; DualSense controls must remain available.
9. Check that saved history, progress, profile settings and library connections remain intact after updating from the prior build.

Report firmware, display/remote type, selected engine, media codec and reproducible steps. Host tests do not prove every console/display/provider combination. Full Dolby Vision processing and HDR10+ dynamic metadata output are not added.
