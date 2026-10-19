# 0.5.34.3 targeted provider test — issue #3

This is a manual test candidate, based on 0.5.34.2, with provider metadata/headers/settings fixes, stream-list improvements and the staged Trakt registration. It is not a normal release/store promotion. Existing release and previous test downloads remain unchanged. PS5 and real-site acceptance remain pending.

## Install

Download `NuvioMaxd-0.5.34.3-native.ffpfsc`, or `PPSA99288.zip` if your installation method uses the app folder. Verify its digest against SHA256SUMS. Fully close the app and use your existing manual installation method; don't overwrite a mounted image. Keep your saved data and only one installed copy. The title ID remains PPSA99288. Confirm app version **0.5.34.3**, numeric package revision **05.000.037**.

## Provider retest

1. Choose the same Nuvio profile used for the original report.
2. Use Settings → Addons → Reload addons and catalogs. Keep the existing repository at `https://invalidpandaa.github.io/nuvio-german-providers/manifest.json`.
3. On the Addons page, open German Providers. Enable one of Moflix, Huhu, FilmPalast or HDFilme first, and try the same title as before. The manifest lists HDFilme as movie-only; start with a movie.
4. Check whether hosts appear and whether one selected result plays with picture and sound. These direct HTTP/HLS sources do not require the torrent switch or a paid debrid service.
5. Test other providers individually. The loading counter should stay visible after a first result appears, without switching to the last provider tab. Reopening a title can show recent eligible results while all sources refresh; query/fragment links are excluded from this short-lived, profile-scoped memory cache.

The app now supplies its own registered TMDB metadata access. Users do not need their own API registration or to enter the maintainer's application key. Top-level provider playback headers are preserved. Addons → repository → Provider settings exposes text/select/toggle options returned by a provider's onSettings callback. Password fields remain unavailable until a verified masked keyboard is implemented. Providers without that callback have no editable options.

## If it fails

Reproduce once, note the provider and exact movie/episode, then open Settings → App & Updates → Playback diagnostics to locate the data directory. Before restarting, download the actual `nuvio/log.txt` using FTP or PS5 Web File Manager and attach it to issue #3. Typical path: `/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt`; your suffix can differ. Remove stream URLs, passwords, account/API tokens, authorization headers and pairing codes. Do not upload settings, account files or a whole data folder. Keep the provider name and complete error/status lines.

## Optional Trakt check

Settings → Account & Services → Tracking accounts → Trakt → Connect with phone / QR code. Use your regular Trakt account to approve the code at the displayed official page. No developer credentials are needed. Connecting doesn't change your library/watch-progress defaults; source choices remain per profile. Live owner-approved device exchange, account validation and refresh rotation passed, and the temporary test token was revoked. Affected-console verification remains pending.

## Validation and limits

See PROVIDER_VALIDATION.json, BUILD-VERIFICATION.json and TEST_RESULTS.txt. Four unmodified reported provider scripts returned streams and playback headers through the production QuickJS/transfer/parse path with simulated HTTP. The owner metadata registration passed a live read-only official TMDB check. Provider/cache/navigation checks, 80 account/profile tests, six PS5 syntax checks and 38 updater regressions passed before packaging. This does not prove current availability of every media website or real PS5 playback.

Existing broader VLC HLS end-of-stream replay/network timeout and Native seek sample trim/gate failures are not claimed resolved by this revision. Updater handoff safeguards from 0.5.34.2 are retained. This manual candidate does not make the normal 0.5.34 release appear as an update or promote a store release. Report only issue #3 provider results here; keep unrelated playback/updater reports in their own issues.
