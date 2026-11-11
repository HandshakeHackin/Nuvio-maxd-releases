# Nuvio Max'd 1.0.0.17

- Add English/German interface selection under **Settings → App & Updates → Language**. Choose **Deutsch**, then fully close and restart Max'd. Menus, settings, dialogs and playback controls use the bundled German catalog, with English fallback. Audio, subtitle and addon-content languages remain separate.
- Add **Cast & crew** to movie/series detail pages: press **R2**, browse the photo row and select a person. Person pages show biography, birth details, known-for department and movie/TV credits. Select a credit to open its title through your configured addons. Circle returns through the browsing history.
- Keep metadata and artwork loading bounded and cancellable, with missing-photo/metadata fallbacks and profile-change guards. Back navigation restores cast/person selection. TMDB requests use the existing app configuration.
- Preserve USB playlist import and private saved copies. The reporter confirmed 1.0.0.16 import/playback, persistence after USB removal/restart, cancellation, profile isolation and valid replacement. Dialog cancellation now uses stable identifiers across translations.

Stable release. Complete app, host/VLC CI, renderer previews, ZIP/native image and matching source verification are required before publication. Console feedback on the two new features is welcome; host verification does not establish every firmware's compatibility.

Fully close Max'd before updating. Keep saved user data and your previous installer; do not overwrite a mounted FFPFSC. Title PPSA99288, contentVersion 07.000.017. The existing optional USB reader remains a privileged payload; Max'd itself stays sandboxed. See the included TESTING.txt and verification report.
