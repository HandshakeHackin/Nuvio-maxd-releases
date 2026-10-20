# VLC sources and local changes

Upstream port: ZiZc3/VLC v0.4, commit in UPSTREAM.json. VLC and libdvbpsi
release sources are fetched by SHA-256 and built privately, leaving SDK files
unchanged. COPYING (GPL), COPYING.LIB and COPYING.dvbpsi (LGPL) retain licenses.

Patches 0001–0017 and modules/ps5vdec.c, compat/*, pkg-config and the static
module generator originate in that port. Local adaptations:

- The RuneLocale compatibility table uses pthread_once for thread-safe initialization.
- The module generator also emits a retention anchor for the static plugin table.
- plugins.txt selects only the modules used by this app; HTTP uses our access bridge.
- 0018 exposes file-declared default track flags through a small private C API.
- 0019 finds fallback subtitle fonts in Nuvio's existing bundled font directory.
- 0020 allows amem float PCM when the PS5 refuses both S16 output formats.

The optional big-picture/copy hooks are explicitly resolved to zero: Nuvio's
existing direct-memory heap and VLC's regular copy loops handle these paths.
The renderer remains OpenGL; hardware decoding uses the port's PS5 module,
with libavcodec software fallback. Native and EVO keep their existing engines.

See ../../VLC_INTEGRATION.md for build instructions and validation limits.

Native MKV playback uses pinned libebml/libmatroska and utf8cpp headers.
All five dependency source archives are retained in the matching source export.

Staged patches 0027–0028 adapt Stremio-Plus 0.5.18 at commit
1475f4c66202325ff5bfde9e715e1dca4420a248 (GPL-3.0): preserve decoder
references when rebuilding a late clock buffer, reject corrupt FFmpeg
pictures, and restore skip_frame after preroll even with hurry-up disabled.
Actual seeks still flush. Rebuild libVLC before packaging these patches;
the existing binary archives do not incorporate source changes automatically.
