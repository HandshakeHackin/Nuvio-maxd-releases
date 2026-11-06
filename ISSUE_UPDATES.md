# Issue follow-up — 2026-11-06

Latest reviewed reply: [issue #10, tester confirmation and player requests](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/10#issuecomment-6683412041).

## Confirmed by the tester

The updated IPTV import works with large playlists. This confirms the import
fix; it does not confirm four-screen playback, Bluetooth sync, or VOD resume.
Keep #10 open for its remaining requests and acceptance checks.

## Player and guide requests

| Request | Current development build | Remaining work |
| --- | --- | --- |
| Select another file/source during playback | Existing source-selection flow | Add a clear player entry point; distinguish another provider source from another file in a torrent. Preserve position for the same title and isolate late callbacks after switching. |
| Stream details including bitrate and resolution | Picture resolution and decoder/playback information are displayed | Add clearly labeled bitrate when available; distinguish container bitrate, measured download rate and torrent throughput. Do not invent missing values. |
| Subtitle size, colour and font while watching | Styling exists in Settings | Expose live style controls from the player, retain profile preferences and preview changes. Explain limits for image subtitles and authored ASS styling. |
| Audio delay in 0.1/0.2-second steps | Player menu already supports 50 ms steps (two/four presses yield 100/200 ms) | Improve discovery and show seconds alongside milliseconds. Retain signed delay and reset. |
| TiviMate-style EPG | Bounded XMLTV/Xtream Now/Next listings, refresh, enable/disable and clock correction | Build a channel/time grid; retain playback when listings are missing, bound large-guide memory use and test navigation/timezones. Now/Next alone does not fulfill this request. |

These remaining UI features are tracked here, not implemented by this sync.
The separate multilingual search keyboard and optional top-navigation request
remains [#19](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/19).

## Source synchronization

Merged main through `441054bbbeff490df1c0e03ac18ed295f72aff65` into
`integration/torrent-upstream-refresh`. The checkout and rebuilt provider runtime
changes from #17/#18 were already present; this merge records shared ancestry
without changing the tested source tree or removing unpublished work.
No release or store publication is part of this synchronization.
