# On-demand trailers — staged

From movie or series details, press **Options** to open **Watch trailer**, then select an available trailer or teaser. Circle stops playback and returns to the same title. Trailers do not autoplay while browsing.

## Discovery and playback

- Reads addon `trailers` and `trailerStreams` metadata, accepting public YouTube identities or direct HTTPS MP4, WebM and HLS trailer URLs.
- Uses Max'd's existing TMDB registration to fetch `/movie/{id}/videos` or `/tv/{id}/videos`. If only an IMDb ID is known, resolves that exact identity first; ambiguous matches are rejected.
- Official English trailers rank ahead of other trailers and teasers. Duplicate YouTube/video identities are removed. At most 12 choices are shown.
- Uses the selected Native, EVO or VLC player. Trailers start at zero with short read-ahead and sequential network playback, rather than the parent movie's saved resume point or parallel download.
- YouTube lookup happens only when a user selects a trailer. Parses supported public watch-page/player responses into a muxed H.264/AAC MP4 or HLS manifest. Separate adaptive audio/video pairs and signature-cipher formats are not supported. No expiring playback URL is cached to disk.

No additional user API registration or personal YouTube login is needed. The existing TMDB build configuration must be present to discover TMDB trailers; addon metadata can still provide trailers without it. No third-party resolver backend, copied application credential, JavaScript execution or signature/DRM bypass is used.

YouTube response formats, regional availability and public playback policies can change. A trailer requiring unavailable signatures, authentication or another unsupported playback method shows an error and leaves the title unchanged. A successful metadata lookup does not by itself verify playable media on a console.

## Session isolation

Trailer playback does not create or change the parent title's watch progress, saved stream, resume time, watched marks or tracker updates. It does not request addon subtitles or intro/recap timings and cannot launch the next episode. Embedded trailer subtitles and ordinary pause/seek controls remain available. The next normal movie, episode or Live TV launch resets the trailer flag.

Leaving the title or changing profile cancels pending requests. Generation and cancellation checks reject stale lists, dropdown selections and resolved URLs. Failures and cancellations are not stored as permanent misses. No playback lookup runs merely from scrolling posters.

## Provenance and validation

Compared the Android [NuvioTV Enhanced](https://github.com/yesnt10/NuvioTV-Enhanced) trailer design at `33097a775120559caa191e3098268c99e4d0b297`. This implementation uses independent C++ parsing and the existing PS5 session/player architecture.

See `TRAILERS_VALIDATION.json` for checks and limits. Live TMDB discovery returned HTTP 200 and five trailers for the test movie. The cloud proxy rejects YouTube CONNECT requests with HTTP 403 before a player response is received, so live YouTube resolution is unverified here. Physical PS5/controller acceptance and final package/link remain pending. This feature is staged alongside Live TV, anime timings, settings search and playback stability; no new public release has been published for it.
