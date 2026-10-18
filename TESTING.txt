# 0.5.34 console testing

1. Confirm Settings shows 0.5.34. Preserve saved data when replacing the app.
2. Use Native and retest the same on-demand/live stream that previously stalled.
3. Compare Continue Watching with mobile using the same account profile and watch-progress source. If titles remain missing, provide one title/episode ID; do not share account credentials.
4. Try a PIN-protected profile: masked numeric keypad, backspace and cancel; do not post the PIN.
5. Check Hebrew titles and subtitles. Existing 0.5.33 downloads remain unchanged.
6. Optionally select VLC in Settings → Video → Playback engine, then reopen a stream. Test repeated 4K opens, seeking, stop/start and interlaced content/audio sync. Native remains default.
7. Check native HDR only on a compatible output; existing pixel-format rejection may still occur. SDR conversion remains available.

For a persistent stall, collect logs before restarting the app. The usual FTP path is /mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt (sandbox suffix may differ). Additional renderer log: download0/hui/dev/app.log in the same sandbox. /download0/nuvio is the in-app sandbox path, not the FTP root. Redact passwords, PINs, tokens, authorization headers and signed URLs.

Host checks and package verification do not prove physical PS5 compatibility. Store listing availability requires maintainer merge/deployment.
