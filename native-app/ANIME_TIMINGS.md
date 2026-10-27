# Anime episode timings — staged

Adapted from [Rasmulus's PR #6](https://github.com/HandshakeHackin/Nuvio-maxd-releases/pull/6), reviewed at commit `0f8bb43ef45541f67757c9dc6887429bae9e148d`, onto the current profile-aware source alongside Live TV. No new package, public release or store update has been published for this feature.

With **Playback → Skip intro and recap buttons** enabled:

- AniSkip supplies anime opening, recap and ending timings without an API key.
- Anime-Skip is optional. Enter its client ID under **Account & Services → Connected Services → Anime-Skip**, or import an existing `animeskip` connection from the selected Nuvio profile.
- ARM maps Kitsu, MyAnimeList and AniList identities. Known MAL/AniList IDs remain usable when ARM is unavailable. IMDb-only shows are checked for an unambiguous anime season when IntroDB lacks an intro; split seasons are not guessed.
- Anime metadata's `imdbSeason`/`imdbEpisode` fields correct the IntroDB lookup while the anime providers retain their own episode numbering.
- The existing controller skip buttons, countdown and post-credit protections apply. Live TV never requests anime timings.

## Safety and integration changes

Anime timings require a finite, positive recorded release length within 20 seconds of the playing file. Anime-Skip may account for explicitly marked opening branding absent from a shorter copy, bounded by the duration difference. Accepted intervals must also fit the actual file. An invalid preferred interval cannot displace a usable fallback. Missing release lengths are rejected.

Anime-Skip, AniSkip and IntroDB results are retained separately and merged by that priority, filling missing types while preserving known post-credit scenes. AniSkip and the optional Anime-Skip requests run independently and publish independently; optional service latency/failure does not hold up the free timings. Empty/failed results cannot erase valid fallback intervals.

Anime state resets for every playback, stop and profile change, including entry into Live TV. In-flight requests are cancelled/ignored after playback or profile changes. Changing/disconnecting the optional client ID invalidates only that service's pending results; other timings remain available.

Connected Services credentials remain in the selected profile's existing mode-0600 settings file. Imported credentials carry the sync owner and are cleared under the existing owner/sign-out rules. Manually entered credentials remain profile-local. Anime-Skip receives only its own client ID over HTTPS without redirects. ARM and AniSkip receive public catalog identities; AniSkip also receives the file duration. No playable URL, stream headers, debrid credential or Nuvio token is sent to these timing services.

## Verification

Focused episode, Connected Services and account/profile fixture tests cover timing bounds, unavailable-source fallback, late-source priority, ambiguous mappings, IMDb numbering, credential isolation and read-only selected-profile imports. PS5 syntax checks cover the affected session/account/service paths. The public ARM and AniSkip endpoint response shapes were verified during review.

A live Anime-Skip client ID has not been supplied for end-to-end service verification. Physical PS5 acceptance and final package/link remain pending. Service coverage varies by anime and release; mismatched or absent data leaves the fallback behavior in place.
