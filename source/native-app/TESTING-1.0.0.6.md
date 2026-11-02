# 1.0.0.6 media-server test build

For [Jellyfin/Emby request #14](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/14). This is a temporary manual-install build, not an official release or an update-feed/store promotion. Plex is not included.

Close Max’d completely. Install the app-folder ZIP or FFPFSC using your normal method, keeping existing saved user data. Confirm **1.0.0.6** under Settings → App updates. Use the same PPSA99288 title; do not delete profile data.

1. Select your Nuvio profile. Open Settings → Account & Services → Media servers (experimental) → Connect Jellyfin or Connect Emby.
2. Enter your reachable LAN server address including its port and base path, followed by the server username/password. Plain HTTP LAN addresses are supported, such as `http://192.168.1.50:8096` (example only). Password input is masked; only the returned token is saved privately.
3. The Media Servers sidebar should appear. Check libraries, movie covers, series/episode order and Triangle search. Options → select your server provides Continue watching, refresh, next/previous pages and reconnect.
4. Play a movie and episode with Native first. Check picture, sound, seeking, pause/resume and embedded subtitle tracks. Stop halfway, refresh Continue watching and verify resume. Check that your server also records the position. If direct playback fails, choose Compatible playback (server HLS); transcoding must be permitted on the server.
5. Restart Max’d and confirm the connection persists. If you have multiple Nuvio profiles, check that connections and pending results do not leak between profiles. Other player choices can be tested afterward.
6. Reply in #14 with the test-build version, PS5 firmware, server type/version, player engine, LAN/reverse-proxy setup and which checks passed or failed. No password, token or full private server URL is needed.

This first version uses a separate Media Servers page. It does not inject server libraries into Board/addon stream results or replace Nuvio/Trakt libraries. External server subtitle delivery and cloud import of server history are outside this initial test. See MEDIA_SERVERS.md for full scope.

If something fails, reproduce it once and collect `log.txt` before restarting. In FTP/file manager, open `/mnt/sandbox/`, then the folder starting `PPSA99288`, then `download0/nuvio/log.txt`. The usual full path is `/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt`; the suffix can differ. Also include `download0/hui/dev/app.log` when available. Settings → App & Updates → Playback diagnostics shows the app's paths. Remove private URLs, tokens, authorization headers and account details. Never upload saved account/profile settings; if access is denied, report the path and error.

PS5 compilation, synthetic Jellyfin/Emby API/Session checks and real authenticated FFmpeg HLS checks passed. Real-server/console acceptance is still pending. Please keep issue #14 open until those results are confirmed.
