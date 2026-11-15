# Age ratings

Movie and series detail pages show published TMDB certifications beside the
year/runtime/genres. **Settings → Account & Services → Age-rating country**
offers 32 countries, defaults to US, and is saved separately for each profile.
The menu and country names support all eight interface languages.

The badge always names the supplying country, for example `ES 12` or
`US PG-13`. When the selected country has no usable certification, the app
uses US. If neither is available it shows no badge. Labels such as `NR` are
preserved when supplied; the app does not invent an age or translate a label
into another country's classification. These labels do not filter playback.

The existing cast request appends `release_dates` for movies and
`content_ratings` for series. No separate request is added. Changing country
reselects from the detail page's cached results. Titles that cannot be resolved
to TMDB, unavailable responses and absent metadata simply have no badge.

Movie certifications prefer theatrical, limited theatrical, digital, physical,
TV, then premiere releases; the first valid label within a type is retained.
Each country/release list has a 256-entry inspection limit. The existing 2 MiB
response limit, request budget and cancellation/deadline remain in force.
Labels are limited to 48 UTF-8 bytes and reject invalid encoding, controls,
hidden direction overrides and URL/markup-like values. The badge has a bounded
width and the surrounding metadata leaves room for it.

Title/profile completion guards reject stale results. Country picker callbacks
also check the selected profile and verify saved settings before updating the
badge. Encrypted configuration backups include the country preference and
reject unsupported values; account sessions and playback history remain excluded.

`tests/people.sh` covers real parsing/request integration, fallback and omission,
theatrical priority, malformed/bounded data, international labels, cancellation,
identity mismatch, country changes during loading, stale profile callbacks,
separate saved profiles and save rollback. `tests/features20.sh` checks the
encrypted backup round trip and rejects invalid country values. Renderer
previews cover movie/series badges, US fallback, absence and translated menus.
