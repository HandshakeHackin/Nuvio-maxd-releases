# Optional native Sports Hub

Reference inspected: [abbaskip/Nuvio-ReSeq](https://github.com/abbaskip/Nuvio-ReSeq/tree/d46b29a47487919057819602b614d5f7fbde7cba), especially its SportsPreferencesDialog, SportsPicker, SportsFollows and public sports-feed design. The native code is independently implemented; derived sport/competition catalogue facts and aliases ship with provenance and the upstream GPL notice in `app/sports/`.

## User flow

Open **Settings → Sports Hub** and turn it on. The Sports Hub navigation item appears for that profile. Off hides it, cancels sports requests and preserves selections. Configuration actions require the feature to be enabled.

- **Choose sports and competitions:** sport → competition → teams. Include/exclude whole sports or competitions; follow whole competitions or individual teams. Team picking supports search and preserves the selected row/query after toggling a follow.
- **Follow teams, leagues and names:** manage saved follows or enter an athlete/team name for whole-word matching against sporting/guide titles. Soccer team IDs match across supported soccer competitions; other leagues remain scoped to avoid ID collisions. Unsupported team directories can use competition/name follows.
- **Coverage region:** prioritise competition/channel candidates and use relevant broadcaster metadata when supplied. All regions retain access to all competitions; free coverage varies.
- **Hide sports scores**, **Only events with channel matches**, and **Sports time display** are separate controls. The time setting is a fixed UTC offset in 15-minute steps; adjust it for daylight saving.
- In the hub, choose All events, My Sports, On now/starting, Upcoming or Results. Triangle searches. Square/Options opens selections. The refresh tile provides paging and reload controls.
- Select an event, review guide/broadcaster candidates, then select a channel. Broadcaster matches are explicitly unconfirmed. Search all loaded channels when needed. No streams are bundled; configure the user’s own M3U/Xtream source under Live TV first. Existing live playback keeps sports out of movie/episode history and tracking.

## Data, bounds and isolation

The build contains 26 sports and 157 unique competition/guide categories. Catalogue membership does not guarantee that the free feed contains fixtures or a team directory for every category.

Public schedule: `https://abbaskip.github.io/Nuvio-ReSeq/sports-base.json`, backed by ESPN, TheSportsDB and sporting-events.org. Valid public snapshots cache for six hours, expire after seven days, reject rollback/future/invalid/oversized data, and retain a valid cache on refresh failure. The single feed is capped at 16 MiB, with bounded parsed events and 60 rendered event tiles per page. Date-only calendar fixtures retain an explicit unknown start time.

Only the focused relevant event can trigger score refresh while the Sports Hub is visible, at most one job per 30 seconds and one league per minute. A job makes at most two adjacent-day requests to handle ESPN’s US calendar around UTC midnight. Cached schedule LIVE states are downgraded; live score labels expire after two minutes. Team directories are requested on demand, capped at 8 MiB/2,000 teams and cached in memory with retry backoff.

Selections live in the existing profile-settings file. Public schedule cache contains no playlist credentials or follows. Private guide/channel matching happens locally. Shared Live TV guide requests retain the existing two-worker and bounded-document/cache limits. Feature/profile/source generations reject stale callbacks and stale channel selections. Off produces no sports schedule, score or team polling. Existing shared Live TV guide work retains its normal lifecycle.

This task adds no compiled app dependencies, changes no release version and publishes nothing. Older downloads, assets and the original workspace remain intact.

## Verification

Evidence and packaged candidate are under `/workspace/nuvio-toolchain/sports-hub/`: production sports policy/session/settings tests, profile/off-state/stale-callback checks, real public feed/team-directory parsing, 16 navigation visibility combinations, focused-event retention, existing HTTP/XMLTV/Xtream guide regressions, and native screen previews (enabled/disabled settings, hub, full rail, sport/team selectors and channel selection). Full native build and image/ZIP/source consistency checks are recorded there. Physical PS5, controller and real subscription-provider verification remain pending.
