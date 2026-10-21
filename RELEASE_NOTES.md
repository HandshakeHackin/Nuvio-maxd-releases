# Max’d 1.0.0

- Added playback badges for resolution, HDR/SDR, source audio channels, codecs and release types such as WEB-DL, Blu-ray and DVD-Rip.
- Added Live TV with M3U and Xtream Codes, channel search, groups, favorites and recent channels.
- Added anime intro/credits timings through AniSkip and optional Anime-Skip.
- Added settings search and a Watch trailer action on movie/show details.
- Recent sources appear faster while fresh results load in the background.
- Fixed audio-track changes losing playback position or pause state.
- Fixed seeking back after playback ends getting stuck buffering; added clearer queue/seek diagnostics.
- Improved audio/video cleanup and optional VLC seek and HEVC recovery.
- Includes Trakt phone pairing, provider, profile, history and updater improvements.

The affected tester confirmed the audio/source fixes in [issue #8](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/8#issuecomment-6294939778). Provider compatibility work relates to [issue #3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3); tracker and updater work relates to [#2](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/2) and [#7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7). Anime timings adapt [community PR #6](https://github.com/HandshakeHackin/Nuvio-maxd-releases/pull/6).

Live TV requires your own playlist/server. Anime-Skip is optional and requires a client ID; AniSkip does not. Some trailers are unavailable when their host requires unsupported playback methods. Native is the default player; VLC remains optional. HDR output depends on the console/display path.

Version 1.0.0. The legacy tag suffix is retained so older in-app updaters can discover this release. Host and package checks passed; console acceptance of the combined new features remains pending. Homebrew Store availability follows catalog approval and deployment.
