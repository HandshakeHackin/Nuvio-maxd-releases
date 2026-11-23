# Native source selection

Adds the most useful MKCinema-inspired improvements to the unpublished Nuvio Max’d development build: source checks, profile-local preferences, transparent ranking and explicit playback of the top-ranked result. No new network service or dependency is required. This is an independent C++ implementation, inspired by the behavior reviewed at MKCinema commit `1009d456d999aa3a9f7845d927089f9e7a602be9`.

## Controls

Settings → Addons → Source preferences, or Source options to the left of the reload/addon buttons on a title’s stream list. Move up from the first stream and left past Reload to reach it.

- Source checks default on; cinema-recording exclusion defaults off.
- Choose minimum advertised resolution, maximum file size (decimal GB), preferred language or HDR preference. Missing metadata passes rather than being guessed.
- Filenames can identify archives/non-video files, likely small samples and clear episode mismatches. Ranges and season packs remain conservative. Anime numbering without a known IMDb mapping is not rejected as a mismatch.
- Cards explain the advertised qualities behind ranking or show an amber warning. These hints do not certify codec support, actual audio tracks, cache state or authenticity.
- Show all sources overrides source filters for the current search. A new search restores filtering. This never enables torrents or unsupported source transports.
- Play top-ranked source is an explicit user action and refuses results carrying a warning or exclusion. There is no automatic failover loop.
- Next-episode preparation/selection uses the same preferences. If nothing qualifies, the existing manual selection path remains available.

Preferences are stored with each profile. Menu callbacks reject profile/search changes and stop pending episode autoplay. Existing progress, release assets and toolchain pins are preserved. Official GitHub/store releases are unchanged.

## Verification

Tests exercise the production policy, menu method and stream-list refresh method (including actual episode matching and playback-index mapping), filename/episode false positives, ranges, packs, metadata absence, preference validation, safe Show all behavior, ranking, stale callbacks, explicit playback and controller navigation. Rendered previews cover the first/last preference rows and ranked stream cards. The full native build and package/source consistency checks are recorded under `/workspace/nuvio-toolchain/source-preferences/`. Hardware verification remains pending.
