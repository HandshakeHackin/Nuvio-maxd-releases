# Issue follow-up — 2026-11-06

Latest reviewed reply: [issue #10, tester confirmation and player requests](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/10#issuecomment-6683412041).

## Confirmed by the tester

The updated IPTV import works with large playlists. This confirms the import
fix; it does not confirm four-screen playback, Bluetooth sync, or VOD resume.
Keep #10 open for its remaining requests and acceptance checks.

## Player and guide requests

| Request | Current development build | Remaining work |
| --- | --- | --- |
| Select another file/source during playback | Playback options can select a snapshot of the current title's loaded sources and resume at the current position | Console acceptance pending; other-title/live-channel navigation still uses its own page. Built-in torrent files have a separate cancellable picker and per-file progress. |
| Stream details including bitrate and resolution | Existing resolution/decoder details now include reported container Mb/s for Native, or measured input Mb/s for VLC | Missing rates are labeled unavailable; measured input is not the encoded bitrate or torrent peer speed. |
| Subtitle size, colour and font while watching | Playback options expose size, font, colour, effect and background with profile persistence | Bitmap subtitles cannot be restyled; authored ASS and VLC may retain their own styling. |
| Audio delay in 0.1/0.2-second steps | Playback options offer signed 100/200 ms adjustments and reset, alongside the existing 50 ms track-menu controls | Console/Bluetooth acceptance pending. |
| TiviMate-style EPG | Optional channel/time grid with a two-hour window, half-hour browsing through the next day, channel filters/pages and live tuning | Uses available XMLTV/Xtream data; no catch-up, recording or future playback. Missing listings do not disable tuning. Tile layout remains available. |

Access playback options with **Up / Options**, then **Square**. Enable the grid
in **Live TV → Sources & filters → Guide layout: Channel tiles**. EPG downloads
remain capped at 32 MiB, with at most 96 listings per channel and 50,000 retained
listings overall. Xtream fetch concurrency and cache bounds are unchanged.

Validation: guide parsing/timezone/network/cancellation tests and clipped grid
interval tests pass. Native playback lifecycle tests pass on an isolated rerun
(the first run failed the existing tight seek-timing assertion under concurrent
build/test load). Production VLC authenticated HTTP/HLS, seek and cancellation
fixtures pass. The console app compiles, and the grid plus track-menu previews
render without GL errors. Physical PS5 acceptance remains pending.

The separate multilingual search keyboard and optional top-navigation request
remains [#19](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/19).

## Source synchronization

Merged main through `441054bbbeff490df1c0e03ac18ed295f72aff65` into
`integration/torrent-upstream-refresh`. The checkout and rebuilt provider runtime
changes from #17/#18 were already present; this merge records shared ancestry
without changing the tested source tree or removing unpublished work.
No release or store publication is part of this synchronization.
