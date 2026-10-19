# 0.5.34.2 temporary test — issues #2 and #7

Based on 0.5.34.1. This is a manual test build, not a normal update or store promotion. It retains your app title ID PPSA99288 and existing saved data. Keep a backup of your current installation and saved data.

Download `NuvioMaxd-0.5.34.2-native.ffpfsc`, or use `PPSA99288.zip` if your existing installation method uses the app folder. SHA256SUMS records each download's digest. Install only one copy using your existing manual method after fully closing the app; do not overwrite a mounted image. The displayed app version must be **0.5.34.2**, with PS5 numeric contentVersion **05.000.036**.

## Issue #7: updater handoff

1. Install 0.5.34.2 manually first. An older running app still uses its older updater helper.
2. Enable your normal PS5 payload loader on port 9021 and ShadowMount+ 1.7's local API on port 10101. The installed FFPFSC must be in a managed `homebrew` folder.
3. Stop playback. Open **Settings → App & Updates → App updates → Test updater handoff (closes app)**, then **Close app and test**.
4. The app should close. Wait up to about three minutes for a PS5 notification before reopening. A passing test means ShadowMount released the backing image. The test does not download, install, replace, or downgrade any image.
5. If it fails, attach `/data/nuvio-maxd-updater.log` and `/data/shadowmount/debug.log` to issue #7. State your firmware, ShadowMount version, install path/method, notification text, and whether `persistent_image_mounts` is enabled. Please copy the updater log before repeating the test, which starts a new log.

With `persistent_image_mounts=1`, ShadowMount intentionally keeps the backing image mounted. This updater preserves the installed image in that case. Set `persistent_image_mounts=0` in `/data/shadowmount/config.ini`, reload ShadowMount normally, then retry. This build never changes that global setting for you and never forces an in-use image replacement.

A successful handoff test verifies the close/release step only. Full image replacement still needs a genuinely newer, verified public release; 0.5.34.2 will not offer older 0.5.34 as an update.

## Issue #2: Trakt

Open **Settings → Account & Services → Tracking accounts → Trakt** after signing in and choosing your Nuvio profile. Regular Trakt accounts should not need personal developer credentials.

**Known remaining dependency:** this default test package contains no owner-registered Max'd Trakt application. It should clearly show **Phone pairing: awaiting app registration**. The missing registration reported in issue #2 is not resolved by this test build; phone pairing cannot be claimed working yet. Do not create an API application or send client secrets/account tokens just to test it. Nuvio Library/Nuvio Sync remain the defaults. Compatible existing synced connections can still be imported; not all Nuvio clients export compatible tracker credentials.

For users who already configured their own application, phone pairing now handles declined/expired/invalid/already-used codes explicitly and retains pending/rate-limited polling. Library and progress choices stay separate for each profile. If those flows still fail, reply to issue #2 with the exact screen message and sanitized `log.txt` from the path reported by Playback diagnostics. A typical path is `/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt`; your mount path may differ. Copy it after reproducing, before restarting the app.

Use FTP or PS5 Web File Manager to download the actual text files and attach them to your issue reply. Remove stream URLs, account emails, passwords, bearer tokens, pairing codes and client secrets. Never attach `tracking.json`, account settings, or a whole saved-data folder.

## Validation limits

Host regressions and PS5 packaging checks are recorded alongside the downloads. Affected-console acceptance is pending. Existing 0.5.34.1 HLS/VLC fixes are retained. The broader previous libVLC 3.0.24 CI network test timed out around HLS end-of-stream seek/replay; this revision does not claim full VLC network compatibility.
