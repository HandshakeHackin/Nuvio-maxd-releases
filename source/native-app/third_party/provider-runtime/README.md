# Provider runtime

QuickJS **2026-06-04**, provided by Payload SDK 0.44, executes each provider in a fresh context. No quickjs-libc, std, os, Node filesystem or process modules are exposed. SDK header matches the official 2026-06-04 source exactly (SHA-256 2165f47772af9faee1798999a599fa9de850d1bf0259502dade1c25d4a588316).

`entry.js` is our host API. `runtime.js` is a reproducible browser bundle of pinned npm dependencies: HTML selectors, CryptoJS, URL and Buffer helpers. Run `bash build.sh` with Node/npm to rebuild. `package-lock.json` pins dependency integrity. Third-party notices are in LICENSES.txt and included in the app image.

Provider scripts are downloaded only when their explicitly enabled provider runs. They receive title IDs and provider settings, never Nuvio account or connected-service credentials. Limits: 64 MB JS heap, 1 MB script, 4 MB response/result, 32 requests, 40 seconds per provider, cancellation on title changes/exit. Network requests validate public resolved destinations and TLS, reject HTTPS downgrades, and strip credentials on cross-origin redirects. Providers needing browser challenges, arbitrary native modules, or private network access are unsupported.

Host tests use the same upstream QuickJS version: download https://bellard.org/quickjs/quickjs-2026-06-04.tar.xz, build `make libquickjs.a`, then run `QUICKJS_DIR=... bash tests/providers.sh` from the native-app directory. The production binary links only core engine objects from the SDK archive.
