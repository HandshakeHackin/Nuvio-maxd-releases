# Live TV and separate IPTV Library

Live TV is included in the published app. New VOD support is staged for the next
build in a separate **IPTV Library** sidebar page; see [IPTV_LIBRARY.md](IPTV_LIBRARY.md).
It reuses imported Xtream accounts or recognized Xtream get.php M3U links and
appears only when movie/series support is found. The Live TV importer still reads
only channels. The historical implementation/validation notes below describe the
original Live TV scope; their VOD exclusion refers to that channel importer.

---

# Live TV — staged for the next build

This feature is staged after the published 0.5.34.3 provider test. No replacement
0.5.34.3 package, public release or store update has been published for Live TV.

## Sources

Open the **Live TV** tab and select **Live TV sources & filters**.

- **Add M3U playlist**: enter a source name and the complete HTTP/HTTPS playlist
  URL. Xtream M3U export links can also be entered here.
- **Add Xtream Codes account**: enter a source name, server base address including
  `http://` or `https://` and port when required, username, then password.
  The app validates the account through `player_api.php`, loads live categories
  and live channels, and builds authenticated live playback URLs. HLS output is
  preferred when the account advertises it; MPEG-TS is also supported.
- **Choose source** switches between saved sources; **Reload channels** refreshes
  the selected service. **Remove current source** requires confirmation.

Credentials are entered using the system keyboard. Password masking has not been
verified on the PS5; this implementation does not claim a masked keyboard.
Credentials are saved locally, without encryption, in the current profile's
`live-tv.json`, using owner-only file permissions. They are not displayed on
channel cards, deliberately logged, synchronized to trackers, or included in
source/build exports. Never attach this settings file to an issue report.

No subscription, built-in channel package, debrid account or torrent switch is
required by the feature itself. Users supply a source they can access. Service
subscription limits and supported codecs still apply.

## Browsing and playback

- **Cross**: open source management or play the focused channel.
- **Square** on a channel: add/remove a favorite.
- **Triangle**: search channel names; submit an empty search to clear it.
- **L1**: choose a category/group.
- **R1**: cycle All, Favorites and Recent channels.
- **Options**: open sources and filters.
- Large results use 500-channel pages. Sources and filters includes Next/Previous
  channel page actions. Group and search changes return to the first page.

The selected Native/EVO/VLC playback setting applies. Live Native/EVO HLS permits
extensionless HTTP segment URLs; nested local-file and other non-HTTP playlist
inputs, and output writes, are blocked. Ordinary title playback retains its
existing filename checks. Live streams use sequential
HTTP/HLS playback with a short read-ahead target rather than the multi-connection
movie downloader. Seeking is disabled. Live sessions do not create movie/episode
history, tracker progress or intro/next-episode actions. Recent channels records
channel selections separately, including a selection whose playback later fails.

Sources, favorites and the 30 most recent selections belong to the current local
profile. Switching profiles cancels pending source requests and clears channel
state before loading the new profile. Limits are 16 sources, 1,000 favorites,
100,000 live channels and 128 MiB per channel-list response. Authentication and
category metadata retain an 8 MiB cap; category lists are limited to 10,000 groups.
Channel-list requests have a 120-second timeout (metadata requests: 30 seconds).

The large-import fix for [issue #10](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/10)
processes Xtream JSON one channel at a time and retains only the fields used by
the live-channel list. It avoids constructing a JSON tree for the entire response.
M3U parsing also avoids a second copy of the playlist. Xtream imports request
only live categories/channels, never the account's VOD or series library.
Cancellation is checked during parsing as well as downloading. Rendering remains
paged at 500 channels; channel/group filtering covers the imported list.

Large-import host fixtures pass with 60,000 channels for both an HTTP M3U over
8 MiB and a gzip-compressed Xtream response over 8 MiB when decoded. Exact
100,000-channel capacity, overflow rejection without partial imports, invalid and
deeply nested JSON, cancellation, redirect handling and credential escaping are
covered. This fix is staged in source; console acceptance and a new installer
remain pending.

## Initial scope and validation

M3U EXTINF names, tvg IDs, groups, relative HTTP links and VLC User-Agent/Referer
options are handled. Duplicate links are removed; unsupported protocols and HLS
segment playlists supplied as channel lists are rejected. Xtream/XUI
`EXT-X-SESSION-DATA` metadata is accepted in channel-list playlists. Network work runs off
the UI thread, uses bounded timeouts and response sizes, and supports cancellation.
Xtream credential-bearing API calls do not follow redirects.

This first version does not add an EPG/program guide, catch-up TV, recording,
Stalker portals, Xtream VOD/series libraries, or DRM playback. Channel logos are
parsed but the initial channel cards use text/number badges.

`tests/live_tv.sh` exercises the real parser/network code against a local fixture
server: imports, categories, encoded credentials, HLS/TS URLs, playback headers,
redirects, rejected login, limits and cancellation. Navigation regressions cover
reaching the sixth tab, source management with no channels and channel arrival.
PS5 syntax checks cover the new source and changed session/UI paths.

Owner-supplied service testing loaded 1,624 live channels and 39 categories
through the production Xtream importer, and the same 1,624 channels through the
production M3U importer. The basic M3U export supplied no groups; Xtream categories
remain available. A real news channel probed as 1280×720 H.264 with stereo 48 kHz
AAC and passed a bounded five-second host decode through each source type.
The Xtream HLS path required accommodating extensionless segments. The production
Native player also passes deterministic HLS variant isolation, extensionless
segments, forced-stall recovery and local-file/write rejection tests. Test credentials and complete responses stay outside Git/builds;
only sanitized results are retained with validation evidence. Physical PS5
playback and final binary packaging remain pending; host decode and fixture
success are not console playback acceptance.
