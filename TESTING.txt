# Testing Max’d 1.0.0.3

Close Nuvio Max’d before updating. Install PPSA99288.zip through Homebrew Store/your usual folder installer, or replace the app image with NuvioMaxd-1.0.0.3-native.ffpfsc using the same image installation method. Preserve saved user data. Title ID: PPSA99288. Package contentVersion: 06.000.003.

- In IPTV Library, try A–Z/All/# and search across saved movie/show catalogs from two providers. Check provider labels, movie playback, series episodes, offline cached results, clearing search and profile switching. Load more categories to populate more searchable saved titles.
- Play a file with embedded chapters in Native, EVO and VLC. Open Options/Up → Chapters, jump while playing and paused, then try a file without chapters and Live TV. Chapter controls should appear only when supported.
- Select alternate audio and embedded/external subtitles or Off; adjust sync/PCM gain, then seek/reopen and check that choices and pause state survive.
- Compare correctly tagged HEVC SDR/HDR and a source with missing container colour tags. Explicit tags must remain authoritative. HDR output acceptance still depends on the console/display.
- Browse a large artwork-heavy catalog, switch profiles and start/stop playback. Check startup on your firmware. Source-level host and PS5 build checks do not establish physical-console behavior.

If a problem occurs, use Settings → Playback log / diagnostics after reproducing it. Include app version, PS5 firmware, player, steps and the diagnostic files or exported log. Remove stream URLs, server logins, passwords and tokens before posting publicly. Existing tracking, updater and provider configurations remain in place.

Host/PS5 build and package verification results are recorded in native-validation.json and TEST_RESULTS.txt. The upgraded PS5 networking libraries still require physical-console execution testing; host network tests use host libraries. Optional VLC/EVO, HDR and multiview features remain experimental.
