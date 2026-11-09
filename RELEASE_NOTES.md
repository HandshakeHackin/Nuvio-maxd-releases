# Nuvio Max’d 1.0.0.14 experimental — possible USB crash fix

- Replace USB scanning/import calls to dup, fdopendir, fstatat and openat with native imported filesystem functions. The pinned SDK emitted raw syscalls for dup/fstatat/openat, a strong candidate for the immediate SYSTEM_ILLEGAL_FUNCTION_CALL crash reported on FW 11.20.
- Keep bounded scanning, playlist validation, private profile copies, atomic replacement, symlink/path checks and file-identity rechecks.
- Add scan/import stage logging without playlist names, contents or private stream URLs. Host regression tests abort if the scanner directly calls the removed APIs.

This is a possible fix, not a confirmed console diagnosis. Host tests and package verification are recorded in the release; physical PS5 confirmation remains pending. Keep the previous installer and saved user data. Fully close the app before installing; do not overwrite a mounted image. Use PPSA99288.zip for a folder install or NuvioMaxd-1.0.0.14-native.ffpfsc for an image install. Content version: 07.000.014.

Please test opening USB import first, then nested playlists, playback, cancellation, unplugging, replacement and reload after USB removal. Report firmware, install type, what worked and the exact step of any remaining crash. The release also includes USB_NATIVE_IO_FIX.md in its matching source archive with the implementation limitations.
