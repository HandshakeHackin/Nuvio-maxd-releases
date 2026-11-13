# Nuvio Max'd 1.0.0.24 experimental — component refresh

- Rebuilt the app with LLVM 23.1.5 and PacBrew 0.41.1. Native, EVO and VLC now share FFmpeg 9.0.2; OpenSSL is 4.1.0. SDK 0.45 and OpenGL 1.0.2 remain pinned.
- Updated EVO sources to 0.13.7 with Nuvio safeguards retained. Embedded libVLC stays 3.0.24; the 0.6.1 console port was reviewed.
- Updated compression, networking, font/subtitle and Matroska dependencies, provider packages, CA trust data, STB, Montserrat and Arabic faces. Inter already matched its latest release.
- Matches the new Arabic font's internal family name and registers the bundled Montserrat subtitle face; regression checks reject silent missing-glyph/default-font fallbacks.
- Keeps buffering active while a demux seek is in progress, so old queued frames cannot release the seek gate early. Paused and superseding seeks have a controlled regression test.
- Preserves the confirmed MDBList fixes from 1.0.0.23. HTML/XML streaming parsers retain compatible Entities 8.1 alongside the newer high-level package because version 9 removed their required decoder API.

**Experimental dependency refresh; console compatibility is unconfirmed.** Please compare Native/EVO/VLC playback, seeking, subtitles, trackers, USB import and existing encrypted backups. Fully close Max’d before updating and preserve saved data and the previous build.

Matching source, dependency provenance, complete package checks and regression evidence accompany this build. Standalone features from the reference apps are not bundled automatically.
