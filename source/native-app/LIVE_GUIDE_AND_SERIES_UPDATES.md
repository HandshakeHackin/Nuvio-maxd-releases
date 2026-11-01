# Staged: Live TV now/next and new-episode indicators

## Live TV programme guide

Live TV channel cards show **Now** and **Next**, with start times and programme titles, when the provider supplies matching listings.

- Xtream accounts use their provider’s short EPG endpoint automatically for channels near the cursor.
- M3U imports recognize `x-tvg-url`, `url-tvg` and `tvg-url` guide declarations. XMLTV channel IDs match the playlist’s `tvg-id`; channel names are not guessed.
- Open **Live TV → Options → Set XMLTV guide URL** to supply or override a guide for the selected source. Accepting an empty entry restores its provider default.
- The same menu contains **Refresh programme guide** and **Programme guide: On/Off**. The toggle is saved for the active profile.
- Programme boundaries update while browsing. Missing or failed guide data leaves channels playable.

Guide work runs in the background and is cancelled when its source or profile changes. XMLTV supports plain XML and gzip, including timezone offsets and escaped titles. The guide document is capped at 32 MiB, held only in memory and reused across channel groups, searches and pages for up to 15 minutes. Parsing retains a bounded near-future snapshot for the current channel page. Xtream requests are limited to two in flight, cached briefly and backed off after failures. Provider URLs and credentials are not logged by the guide feature.

## New-episode indicators

Series in the active Library can show a badge such as **New: S2 E3**. Metadata checks run for nearby Library series, and available badges can also appear on matching Board cards.

A badge requires:

1. A successfully synchronized watched-history snapshot for the active profile and progress source.
2. At least one known completed regular episode with matching metadata identities.
3. A later, unwatched regular episode with a valid release date that has passed.

Future episodes, missing/invalid dates, specials, unknown history and episodes already in progress do not generate badges. Date-only releases wait until that UTC calendar day has ended. The badge identifies the first qualifying episode after the latest completed one. Watching or changing watched marks refreshes the indicator; switching profiles or tracker sources clears cached state and discards stale results.

Metadata requests are deduplicated and limited to two in flight, with bounded in-memory caches, hourly successful refreshes and failure cooldowns. The feature reads metadata and history; it does not change Library membership or watched marks.

## Validation and delivery

Provider HTTP fixtures cover XMLTV/gzip, timestamps/timezones, programme boundaries, channel identity, malformed XML/entity declarations, cancellation, size limits and the Xtream request contract. Production Session tests cover request bounds, guide document reuse, disabled guides, playback independence, outages and stale source/profile replies. Series tests cover aired-only badges, aliases/canonical episode identities, partial progress, manual watched marks, missing history, cache expiry and stale callbacks. Library search and cover regressions, large channel imports, rendered previews and PS5 compile/link/import checks are recorded in `LIVE_GUIDE_SERIES_VALIDATION.json`.

These features are staged with Library search and the two reporter-confirmed fixes. Published 1.0.0.4 test artifacts remain unchanged; use a new version for delivery. Physical-console/provider testing of the new features is still pending.
