# IPTV Library — staged for the next build

IPTV Library is a separate sidebar page for movies and series from an imported
IPTV service. The existing Live TV, Discover, Library and addon movie/TV source
paths remain separate. This feature is not part of the published 1.0.0.1 package.

## Availability and imports

Add the service under **Live TV → Live TV sources & filters**. Xtream accounts
and recognized Xtream `get.php` M3U links are checked in the background for movie
and series categories. Generic M3U channel playlists are not assumed to contain
VOD: they have no reliable VOD/season metadata. These keep their existing Live TV
behavior. Credentials are reused locally; users do not need a second login.

- The IPTV Library tab stays hidden until the current profile has a confirmed
  VOD-enabled service. Only the content types that service exposes can be selected.
- Known capabilities are saved per profile. A temporary outage or rejected check
  does not hide an existing library or erase its saved categories/catalog.
- A successful check confirming neither movies nor series hides that service's
  library. Removing the last VOD-enabled source hides the tab; other Live TV
  services remain usable. Pending replies cannot restore a removed tab/profile.
- Existing installations are checked on profile load. Live TV's **Check IPTV
  Library availability** retries discovery, including while the tab is hidden.

## Browsing and playback

The new page has **Server**, **Type** (Movies/Series) and **Category/Season**
filters. Categories load before content. Initial browsing chooses a category;
**All categories** requests the provider's full list. Large lists use 100-title
pages. Triangle searches the loaded category; choose All categories first for
provider-wide movie/series search. Options/Square opens provider, category,
refresh, retry, page and cancellation controls.

Cross on a movie plays that provider's authenticated movie endpoint directly.
Cross on a series loads its episode details only then; use the Season filter and
Cross to play an episode. Circle returns from episodes to the series catalog.
A saved position offers **Resume**, **Play from beginning**, or **Cancel**.

Native/EVO/VLC and the existing audio/subtitle controls apply to these direct
streams. Native/EVO IPTV VOD uses sequential HTTP requests rather than the normal
parallel movie downloader, respecting services with limited simultaneous
connections. Seeking still depends on the provider's HTTP/range support.

## Isolation and storage

Provider source IDs, content type and item/episode IDs form separate `iptv:`
identifiers. IPTV playback does not look up addon sources, addon subtitles or
IMDb episode timings, and never enters normal Nuvio Continue Watching, the Nuvio
library, Trakt, SIMKL or other tracker history. In-file subtitles still work.
IPTV resume/completed markers are local to the active profile and provider.
Automatic next-episode playback and cloud matching/sync are not added here.

`iptv-library.json` stores capabilities, selection and at most 500 progress
entries with owner-only permissions. It contains no account password or stream
URL. Source credentials remain in the profile's existing `live-tv.json`; never
share that file. This is local permission protection, not encryption.

Successful catalogs are saved atomically as bounded per-profile JSONL caches.
Cache keys include the source, content type and category/series; file headers
are checked before use. Caches keep entry metadata rather than authenticated
playback URLs. Credential-bearing artwork URLs are rejected. Cached catalogs
appear first and refresh in the background; failed refreshes retain the previous
catalog. Removing a source clears its catalog cache and local progress.

## Bounds and testing

- 128 MiB per movie/series category response, up to 100,000 entries, parsed one
  small entry at a time; no full catalog JSON tree.
- 8 MiB for authentication/categories/one series' episode details, at most 10,000
  categories or episodes. Nested metadata is bounded to 64 levels.
- Cache files at most 32 MiB each, total target 128 MiB per profile; older catalogs
  are evicted. Oversized/failed cache writes retain the previous file.
- Account/category queries do not follow redirects, so credentials are not
  forwarded to another host. Playback still follows the provider's normal media
  handling. Errors omit URLs and credentials. Download/parse work is cancellable.

Host tests use synthetic accounts and an HTTP fixture server, not a subscriber's
real service. They cover parser bounds, credentials, hidden/live-only discovery,
movies and lazy episodes, direct playback, local resume isolation, cache recovery,
profile/source races and removal. Navigation checks cover enabling/hiding the tab
without changing existing routes. PS5 compile checks and the UI preview are
separate from physical-console/provider acceptance, which is still required.

### Provider availability messages

When VOD playback fails with an HTTP server error (5xx), Max’d identifies the
IPTV provider's server response and explains that its service may be experiencing
an outage. This is not proof of a provider-wide outage. Network timeouts advise
checking the internet connection and retrying; rejected access, missing titles,
and request/connection limits each have separate guidance. Decoder and PS5 output
errors are never relabeled as provider outages. Library request failures also show
provider guidance and retain any saved catalog. Messages do not echo authenticated
URLs or provider credentials. Hardware validation remains pending.
