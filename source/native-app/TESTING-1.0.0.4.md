# Max’d 1.0.0.4 — targeted issue test

This prerelease includes fixes for issues #12 and #13. Version 1.0.0.3 remains the stable release. Console confirmation is pending; please test only if you are comfortable installing a prerelease.

## Installation

1. Close Max’d before replacing its application files. Preserve your saved user data and IPTV/account settings.
2. Folder installs: download `PPSA99288.zip`, extract it, and replace the existing application folder with the complete `PPSA99288/` folder. Avoid an extra nested folder level. Image installs: replace the old app image with `NuvioMaxd-1.0.0.4-native.ffpfsc` in the location scanned by your homebrew loader. Keep only the intended app copy in that scanned location.
3. Launch and check Settings → App updates reports **1.0.0.4**. The package contentVersion is **06.000.004**; title ID remains **PPSA99288**.

## Issue #12 — subtitle size

Use the same video and external text subtitle (for example, OpenSubtitles) that previously appeared too small. In Settings → Subtitles → Subtitle size, compare Small, Medium and Large with playback, then try Extra large (150%) and Largest (200%). The percentages are relative to Medium. Confirm the size increases during playback and the settings preview is representative. Existing saved sizes are retained; authored ASS styling/positions and bitmap captions keep their existing behavior.

Please reply to [#12](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12) with whether the problem is fixed, your firmware, playback engine and subtitle source/format. A photo is helpful if the preview and actual playback still differ.

## Issue #13 — Trakt Library covers

With Trakt selected as Library Source, restart the app and go straight to Library without opening the missing titles on the Board first. Leave the cursor on the affected titles briefly to allow their metadata and artwork to load, then scroll farther through the Library. Covers should load independently of having viewed the title elsewhere. Existing covers are retained. Compatible metadata addons must remain enabled and reachable; requests are restricted to the nearby title window and four concurrent lookups.

If you use multiple profiles, switch profiles and confirm covers remain associated with the correct titles. No account reset, library deletion or new Trakt connection is required.

Please reply to [#13](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13) with whether the missing covers now load, the affected movie/show titles and enabled metadata addons if any remain missing.

## Logs if either issue persists

Use Settings → App & Updates → Playback diagnostics (or search Settings for “diagnostics”). Reproduce the problem on this test version, then attach the app's `download0/nuvio/log.txt` and `download0/hui/dev/app.log` if present. In an FTP/file manager these normally sit below `/mnt/sandbox/PPSA99288_000/`; sandbox suffixes may vary. Include the version line. Do not post account tokens, passwords, private playlist links or stream URLs; redact those before uploading. A screenshot of the log-location screen is useful if files cannot be found.
