# Max’d 1.0.0.7 test build

This combines the staged playback/subtitle features, Jellyfin/Emby and Plex support, and the latest selected dependency maintenance. It is a local test candidate, not an official public/store release.

Close Max’d completely and install either PPSA99288.ffpfsc or the extracted PPSA99288 folder from the ZIP using your usual method. Keep existing user data. Confirm **1.0.0.7** in Settings → App updates. The title remains PPSA99288; contentVersion is 07.000.007.

## Test checklist

- Normal addon movie/show playback: Native, EVO and VLC; audio, subtitles, pause, repeated seeks, resume and completion.
- Media servers: Settings → Account & Services → Media servers. Connect Jellyfin/Emby with server URL and credentials. For Plex, scan the QR code or enter the displayed code at plex.tv/link, approve and select your server. Browse/search, start/resume, try Compatible playback (server HLS), stop, then verify the position in your server. Change Nuvio profiles and check account isolation. Plex Home managed-user switching is not included.
- Live TV: M3U/Xtream playback, guide loading and provider guide-clock correction. Existing IPTV Library VOD and multiview remain experimental.
- Optional settings: PCM Night Mode and stereo bass, chapter skip labels, preferred trailer language, and subtitle auto-sync (Native/EVO only; needs a reliable embedded text reference and remains off by default).
- Subtitle rendering and multilingual text, including Hebrew/Arabic/Thai/CJK where available. Check HDR10-compatible Dolby Vision profile 7 base-layer playback in Native/EVO. This does not add Dolby Vision output or a new TrueHD audio transport.

The package uses FFmpeg 7.0.3, Expat 2.10.0, libpng 1.6.59, FreeType 2.15.0, HarfBuzz 15.0.0 and FriBidi 1.0.18, plus five pinned libass fixes. OpenGL/SDK and existing curl/OpenSSL release lines remain unchanged. See RELEASE_NOTES_NEXT.md, MEDIA_SERVERS.md and DEPENDENCY_UPDATES_NEXT.md for details.

If anything fails, note the selected engine, audio/output settings, exact action and playback time. Use Settings → App & Updates → Playback diagnostics to locate the diagnostic files, and provide runtime/app/boot logs where available. Leave out passwords, account tokens and signed stream URLs. Existing receipts describe automated/host checks; real PS5/server acceptance remains pending.

## Issue #14 password-keyboard retest

On firmware 7.00, open Settings → Account & Services → Media servers → Connect Jellyfin. Enter the same LAN HTTP server address and username, then check that the password keyboard opens and masks input. Submit the password, confirm login and library browsing, then try a movie/episode and verify resume after stopping. Basic Latin is selected only for secret fields; normal text fields retain the default IME type. Password masking, no-learning, no-auto-capitalization, no-resume and no-auto-space flags are retained. No unmasked fallback is used.

If keyboard opening still fails, share only the fresh `ime: sceImeDialogInit ...` line and app version/firmware. For later login/playback failures, provide the error text and sanitized logs. Do not share the password or account/settings files.
