# Provider runtime

QuickJS **2026-06-04**, provided by Payload SDK 0.44, executes each provider in a fresh context. No quickjs-libc, std, os, Node filesystem or process modules are exposed. SDK header matches the official 2026-06-04 source exactly (SHA-256 2165f47772af9faee1798999a599fa9de850d1bf0259502dade1c25d4a588316).

`entry.js` is our host API. `runtime.js` is a reproducible browser bundle of pinned npm dependencies: HTML selectors, CryptoJS, URL and Buffer helpers. Run `bash build.sh` with Node/npm to rebuild. `package-lock.json` pins dependency integrity. Third-party notices are in LICENSES.txt and included in the app image.

Provider scripts are downloaded only when their explicitly enabled provider runs. They receive title IDs and provider settings, never Nuvio account or connected-service credentials. Limits: 64 MB JS heap, 1 MB script, 4 MB response/result, 32 requests, 40 seconds per provider, cancellation on title changes/exit. Network requests validate public resolved destinations and TLS, reject HTTPS downgrades, and strip credentials on cross-origin redirects. Providers needing browser challenges, arbitrary native modules, or private network access are unsupported.

Host tests use the same upstream QuickJS version: download https://bellard.org/quickjs/quickjs-2026-06-04.tar.xz, build `make libquickjs.a`, then run `QUICKJS_DIR=... bash tests/providers.sh` from the native-app directory. The production binary links only core engine objects from the SDK archive.

## Owner metadata registration and provider options

Set `NUVIO_PROVIDER_APPLICATIONS` to a private JSON file containing
`{"tmdb":{"api_key":"<your v3 application API key>"}}` when building. The
validator accepts the owner's 32-character v3 application key only, not a user
account token. Never commit the populated file or include it in source archives.
Like other distributed API clients, the app image must contain its application
registration; it is not an account password. The checked-in default is empty.
A release intended to support metadata-dependent providers must supply a valid
owner registration. Metadata credits are shown in App & Updates.
Packaging fails without that registration by default; development builds can
explicitly opt out with `NUVIO_ALLOW_UNCONFIGURED_PROVIDERS=1`.

The host injects the owner key into `TMDB_API_KEY`; a provider's explicit
`tmdbApiKey` option can override it. Missing/rejected metadata authorization is
reported even when provider code catches errors and returns an empty array.
No URLs, keys, request headers or raw provider console messages are logged.

Addons → repository → Provider settings loads the provider's `onSettings`
callback. Text, select and toggle options are stored by repository and provider
within the current profile and passed to stream execution. Inheriting profiles
manage these options in the primary profile. Password fields are not exposed
until a verified masked keyboard is available. Repositories without an
`onSettings` callback need no manual options.

Successful script downloads are cached for five minutes, keyed by resolved URL
and provider version; at most eight scripts of 1 MiB each are retained. Reload
addons clears this cache. Stream-list warm starts are session-memory-only,
bounded to eight titles and 256 direct links per title for sixty seconds.
Query/fragment URLs are excluded, source/settings/profile identity is part of
the key, and every warm start refreshes all sources in the background. Fresh
answers replace that source's cached links; failed searches invalidate the
cached list. Changing profiles clears the cache. Loading progress remains
visible after the first stream appears.

For the four reported German providers, run `tests/providers.sh` first, then
`QUICKJS_DIR=... bash tests/providers_german.sh moflix.js huhuto.js filmpalast.js hdfilme.js`.
Supply the unmodified public scripts from the reported German repository. The
fixture runs the production QuickJS/download/parse path with simulated metadata
and website responses, checks returned streams and playback headers, and checks
rejected metadata authorization. It does not access media sites or prove PS5
playback availability.
