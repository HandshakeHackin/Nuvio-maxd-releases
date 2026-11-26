# Issues #28 and #29: 1.0.1.2 test

Unconfirmed, unsigned PS5 test based on 1.0.1.1. Keep your saved data and previous installer. The dismissible **Unverified build** notice is expected. Stable 1.0.1.0 remains available.

## #29: German providers

The issue screenshot shows a missing KellerKino script (HTTP 404), before playback. A cached repository manifest can keep an old path after a provider renames a script. This build refreshes the user's original repository after a 404/410, looks for the same unique provider ID, and retries its current script path once. The actual reporter's repository URL and root cause are not confirmed.

Reload sources in Settings and retry the same German providers. Report whether the HTTP 404 disappears and whether results load. If it persists, share the public repository/manifest URL and the visible error, without credentials or private links.

Saved enabled choices and provider settings stay intact. The retry keeps public-address/TLS/redirect checks, cancellation, one-MiB manifest/script limits, 256 manifest entries and a shared 15-second deadline. Removed or ambiguous IDs and authorization failures are not retried against other repositories.

## #28: USB torrent cache

1. Enable the console's existing payload loader on port 9021 and connect a writable USB drive.
2. Open **Settings → Torrent settings → Cache location** and select **USB 0** (or the mount for your drive). Keep the existing cache-size and read-ahead choices.
3. Play a torrent you are authorized to access. Check playback, seeking and pause/resume, then open the torrent settings' cached-storage row to inspect usage or a RAM-fallback message.
4. Stop playback, restart the app and check that the location remains selected. Test **Internal SSD** as a comparison. Report console model, firmware, USB filesystem/mount, cache size and any cache warning. Do not post media URLs, hashes or account files.

Changing location/size during playback is refused. Only the selected USB mount is used. If it is missing, read-only, removed, full, or the helper is unavailable, the rolling cache uses at most 384 MiB of RAM; it does not silently write the rolling buffer to the internal SSD. Lost verified pieces are requested again with the existing cancellation/deadline policy. Playback can still buffer or fail if the source cannot supply those pieces.

The new helper uses the existing local loader connection; it opens no listener and makes no network requests. Requests contain only a mount number, capacity and bounded offset/length operations. Each connection creates its own anonymous temporary file in `.nuvio-maxd-cache`, unlinked before any buffered media is written. It follows no mount/directory symlinks, never truncates existing files, and deletes no directory contents. Normal closure, profile switching or loss of the connection releases the buffer. USB buffering is not encrypted; it is temporary media data, not account credentials. Settings, torrent metadata and history remain in app storage. Any older internal cache is preserved until explicitly cleared with Internal SSD selected.

Cache capacity remains 1/2/4/6 GiB. IPC uses at most 256 KiB per chunk, a five-second operation deadline and one-second socket I/O timeout. The read-only USB playlist helper is unchanged. No raw SDK filesystem syscalls or mount/credential changes were added.

## Verification

Regression coverage includes the actual provider request/execute pipeline and stale-path recovery; the production cache helper/client over a loader socket fixture; sparse six-GiB bounds, invalid protocol messages, isolated sessions, anonymous cleanup, missing mounts, directory symlinks and a killed helper; and real torrent-engine verified-piece recovery on cache open/write/read failures with an unchanged internal-cache sentinel.

Delivery requires the complete PS5 app build, matching ZIP/native-image/source checks, existing functional/player regressions and honest hosted CI evidence. These checks do not establish physical console compatibility. Pinned dependency versions remain unchanged; the dependency audit's proposed upgrades are separate work.
