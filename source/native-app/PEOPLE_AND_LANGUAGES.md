# 1.0.0.17: interface language and people browsing

## Interface language

The local settings.json stores ui_language as en or de. Unknown/malformed values
fall back to English. The selector verifies saved readback and asks for a restart;
there is no partially switched set of cached screens. Profiles and audio,
subtitle, trailer and provider language settings remain separate.

ui/locales/de.json is the translation source. tools/build_locales.py validates
placeholders and produces the reproducible de.inc compiled into the app. The
build checks that the generated table is current. Explicit lookups cover app-owned
text; user names, filenames, URLs, addon metadata, JSON fields and protocol IDs
are not translated. A missing translation returns English. German settings search
folds case, umlauts, sharp S and combining accents without changing process locale
or protocol/numeric parsing.

Dialog purposes and the Continue Watching row flag carry behavior independently
of labels. This preserves USB cancellation and resume-card rendering in German.
Secret/PIN input paths and their masking remain unchanged.

## People browsing

Native C++ views implement the behavior requested in issue #21. The reference is
NuvioMedia/NuvioTV's GPL-3.0 dev branch at
b8c166824e01fa63633343e6d53471c31ce76c90, particularly PersonDetail.kt and
TmdbMetadataService.fetchPersonDetail. The Android UI is not bundled.
https://github.com/NuvioMedia/NuvioTV/tree/b8c166824e01fa63633343e6d53471c31ce76c90

The existing owner TMDB configuration supplies authentication. Requests stay on
api.themoviedb.org with fixed endpoint construction, validated numeric IDs and en/de
language values. Source title/person identities are checked in responses. Public
image paths are restricted to single TMDB image filenames on image.tmdb.org.
No playlist, profile path, watch history, account token or playback URL enters
these requests. Error messages do not expose response bodies or API credentials.

Movie credits, TV aggregate roles/directors and TV creators produce at most 80
unique people. A director who also acts retains the character label. Person
requests append combined credits, select cast or crew according to the entry,
and fall back to the other list when absent. German biography lookup can fall
back to English. Each movie/TV list is deduplicated, bounded to 256 and sorted by
year. Names and supplied original titles remain metadata, not UI translation keys.
Metadata replies are capped at 2 MiB; each fetch allows at most four requests and
stops starting requests after its 25-second budget. The existing HTTP timeout and
cancellation also bound the active request; platform I/O can defer cancellation.
Repeated Retry while a request is pending does not queue more requests.

Cast loading can begin from a title ID before addon metadata completes. If an
addon later supplies a usable TMDB ID, the failed initial lookup may be retried
once with that ID. Worker completions check cancellation, title generation and
profile generation before changing visible data.

Credits resolve through TMDB external IDs to IMDb, with supported TMDB title IDs
as a fallback. Only configured addons that support the title receive metadata/
playback requests; there is no guessed identity or automatic addon installation.
Unsupported titles remain on the person page with a message. Browsing does not
write watched history. Existing artwork queues/cache handle only the visible
windows of images.

The shell keeps at most eight lightweight navigation frames. Person snapshots
contain public metadata; title frames reopen the configured metadata path and
restore cast selection. Actor/person focus is retained on Back. Profile changes
clear navigation and cancel outstanding requests. Restored images start with zero
texture handles and refresh through the cache, avoiding stale GPU IDs.

## Verification

people.sh exercises real metadata parsing/requests with fixtures, controller
views, bounded history and production Session methods with task/network boundaries.
It covers identity mismatch, movie/TV roles, duplicate people, bounds, traversal/
image rejection, English fallback, cancellation, stale completions, profile
switches and configured-addon opening. localization.sh checks persistence, invalid
settings, fallback, German search/fonts and production dialog/language callbacks,
including failed-save readback and stable USB cancellation purposes.

The renderer preview suite includes both languages, sign-in/PIN, USB, cast/person
views and the last cards of long rows. A live smoke check validated cast/person
metadata, portrait access and IMDb mapping with the existing app configuration;
credentials are excluded from the evidence. Host tests and rendering do not
establish firmware compatibility. Console feedback is requested after release.
