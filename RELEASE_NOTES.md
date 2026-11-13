# Nuvio Max'd 1.0.0.24 experimental — component refresh

- Rebuilt the app with LLVM 23.1.5 and PacBrew 0.41.1. Native, EVO and VLC now share FFmpeg 9.0.2; OpenSSL is 4.1.0. SDK 0.45 and OpenGL 1.0.2 remain pinned.
- Updated EVO sources to 0.13.7 with Nuvio safeguards retained. Embedded libVLC stays 3.0.24; the 0.6.1 console port was reviewed.
- Updated compression, networking, font/subtitle and Matroska dependencies, provider packages, CA trust data, STB, Montserrat and Arabic faces. Inter already matched its latest release.
- Matches the new Arabic font's internal family name and registers the bundled Montserrat subtitle face; regression checks reject silent missing-glyph/default-font fallbacks.
- Preserves the confirmed MDBList fixes from 1.0.0.23. HTML/XML streaming parsers retain compatible Entities 8.1 alongside the newer high-level package because version 9 removed their required decoder API.

**Experimental dependency refresh; console compatibility is unconfirmed.** Please compare Native/EVO/VLC playback, seeking, subtitles, trackers, USB import and existing encrypted backups. Fully close Max’d before updating and preserve saved data and the previous build.

Matching source, dependency provenance, complete package checks and regression evidence accompany this build. Standalone features from the reference apps are not bundled automatically.

Previous release notes follow.

---

# Nuvio Max'd 1.0.0.23 experimental — explicit episode history and steady carousel

- Corrects the 1.0.0.22 regression that treated MDBList show/season activity summaries as episode completion. Only explicit episode records set episode checks, even when a summary's watched/total counts match.
- Retires the old read-side scope cache locally without changing explicit marks, resume positions or tracker write queues.
- Watched-only refreshes preserve the current episode, season focus and carousel scroll position.
- Retains the confirmed library sync and movie watched indicators.

**Unconfirmed fix for the latest #22 report.** Please test individual episode marks, later seasons, revisiting/restarting and marking a card after scrolling right. Matching source, complete app/package checks and regression results accompany the build.

Previous release notes follow.

---

# Nuvio Max'd 1.0.0.22 experimental — watched history and movie badges

- Fixes MDBList episode-history nesting and reads all bounded offset/cursor pages before replacing local watched state.
- Supports dated whole-season/show marks while excluding future/unknown episode dates; individual unmarks override unchanged parent marks.
- Movie watched checks now follow watched history across Home, Library, Discover and Search, with a translated Watched badge on details. Manual toggles refresh immediately and keep resume positions.

**Unconfirmed fix for the remaining watched-state problems in #22.** Please test episode checks after leaving/reopening a series and restarting, movie watched/unwatched badges, and the result on mdblist.com. The 1.0.0.21 library add/remove fix is retained and was confirmed by the reporter on firmware 7.00 ZIP installs.

Previous release notes follow.

---

# Nuvio Max'd 1.0.0.21 experimental — MDBList library sync

- Recognizes MDBList add/existing/remove confirmations separately from Trakt.
- Checks the remote watchlist before retrying pending changes; supports bounded offset pagination and guarded ID-format compatibility.
- Preserves profile isolation, offline queues, watched history and resume positions.

**Unconfirmed fix for #22.** Please test movie/series add and remove, restart, and the result on mdblist.com. Full app/package and regression checks accompany the build; live MDBList/PS5 confirmation is still needed.

Previous release notes follow.

---

# Nuvio Max'd 1.0.0.20 — configuration backup, TVDB ordering and portals

- Encrypted per-profile configuration backup/restore, including private imported USB playlists. AES-256-GCM with PBKDF2-HMAC-SHA256; rollback recovery; no account sessions or playback history in backups.
- Manual Google Drive configuration sync with three-way conflict review, conditional updates and upload verification. Google OAuth TV application setup is required; no registration credentials are bundled.
- Per-series TVDB episode order selection using your own API key/PIN. Source searches and next-episode navigation follow the selected order; watched/history/resume identities remain canonical. Ambiguous split/combined mappings retain provider order.
- Stalker/Ministra portal sources with provider-assigned MAC and optional login/serial. Live channels, signed playback links, guide and available movies/series use bounded, cancellable requests. Portal credentials are never forwarded to media CDNs.
- New menu entries translated into all eight supported languages; advanced help may appear in English.

These features are implemented and regression-tested, but live account/provider behavior and physical PS5 operation are **unconfirmed**. Please use the testing checklist before relying on them. Existing USB diagnostics/import validation, profile isolation and private atomic copies remain in place.

No playlists, media, account access, TVDB API subscription or Google OAuth app registration are supplied. The app remains independent of its metadata, account and content providers. Metadata: TheTVDB.com and TMDB, without endorsement.

Previous release notes follow.

---

# Nuvio Max'd 1.0.0.19 — detail actions and controller hints

- Options on a movie/series detail page opens Title actions: add/remove library membership, movie watched/unwatched, series/season watched actions and trailers.
- Library changes sync to the Nuvio profile and the supported selected Library tracker (Trakt/MDBList). SIMKL library remains read-only here; its history is not removed by a library action. Watched marks use the selected Watch Progress service.
- Bulk actions show a confirmation/count and affect listed released episodes, with bounded scanning and no silent truncation. Offline changes are retained separately from playback progress.
- Triangle's episode watched action is now shown in the hints. Cast/person screens use controller icons, with Circle/R2 on the cast row.
- New action labels are translated in all eight menu languages. Existing USB import, profiles, playback and menu-language support are retained.

Real-account sync and PS5 behavior for the new actions need confirmation. Resume positions remain separate from manual watched marks. Fully close Max'd before updating, preserve saved data and do not overwrite a mounted FFPFSC. Title PPSA99288; contentVersion 07.000.019.
