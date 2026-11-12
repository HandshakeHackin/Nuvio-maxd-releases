# Detail actions (1.0.0.19)

Press **Options** on a movie or series detail page for **Title actions**.
Add/remove library membership, mark a movie watched/unwatched, mark a series or
selected season's released episodes, or watch a trailer. **Triangle** still
marks the selected episode on the episode carousel and reloads stream results
in the streams view. The episode hints now show the watched action.

Library changes always use the signed-in Nuvio profile's membership RPCs and
also the selected Library tracker's watchlist when supported (Trakt/MDBList).
SIMKL's tracked library is read-only here: removing it can remove history, so
this build reports that limitation and only changes Nuvio membership. It does
not silently call a destructive SIMKL history endpoint for library removal.
Watched changes use the selected Watch Progress service (Nuvio/Trakt/SIMKL/MDBList).
These actions do not fabricate playback or erase saved resume positions.
Guest watched marks remain local. Guest library actions require sign-in.

Series/season bulk actions have a confirmation with an exact count, exclude
future/unknown release dates when marking watched, and can clear already-known
marks with missing dates. Date-only releases become eligible after that UTC day
has ended. Only the metadata provider's listed episodes are included, up to
5,000 per action; oversized scopes are rejected rather than truncated. Individual
episode marking remains available for episodes without a release date.

Nuvio library membership has its own private atomic cache/queue, distinct from
native progress files. Upserts/deletes send only the selected title keys, never
a replacement of the whole remote library. The latest pending intent per
(type, ID) wins; there are at most 128 intents and 10,000 snapshot entries.
Incomplete/malformed snapshots retain the previous cache. Unreadable saved state
is retained and blocks mutation. Queued changes retry on library refresh, which
runs while the app is open. Tracker queues stay bound to the original connection
and are cleared when it is replaced/disconnected. Requests use fixed HTTPS
origins and portable metadata, with no playback URLs/headers or addon credentials.

Watched batches validate all entries before writing, merge existing remote
Nuvio history and retain offline intents. Explicit tracker history marks are
batched in groups of at most 100; scrobbles keep their own ordering. A rejected
batch is retried individually to identify the rejected title. Profile/context
and service checks reject stale mutations; UI callbacks also reject profile and
tracker-generation changes. Cancellation before confirmation has no mutation.

Cast and person pages now use the shared dark controller glyphs. The cast row
shows D-pad, Cross, Triangle, Circle/R2; person pages show D-pad, Cross, Triangle
and Circle. New action labels are translated in all eight menu languages;
advanced explanations can use English fallback.

Protocol reference inspected: NuvioTV commit
11689894f02ddc7f81c513abc1de1afe9247db1c, especially
SupabaseLibrarySyncRemoteDataSource and MdbListLibraryWriter. Native implementation
and tests are included in this source. Existing GPL notices are retained.

Checks: production menu/callback tests, scoped metadata selection, account HTTPS
fixtures, tracker contract fixtures, native compilation, renderer previews and
ZIP/image/source-archive consistency. Real-account round trips and physical PS5
acceptance for these changes require tester feedback.
