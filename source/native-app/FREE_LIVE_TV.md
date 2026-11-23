# Free live TV

Open **Settings → Live TV → Free live TV**. Read the disclaimer and choose
**I understand — enable free TV**. Pick **Free TV countries**. Tick as many countries as you want, then choose
**Apply** (or press Square). **Select all** and **Clear selection** help with longer
lists; Circle or **Cancel** discards changes. Then choose
**Open free live TV**. To include your personal sources in the same guide,
open **Settings → Live TV → Unified TV guide**. No IPTV subscription, provider account, API key or
Cloudflare setup is required. Guest profiles can use it too.

The switch is off by default and belongs to the selected profile. Your own
M3U playlists, Xtream accounts and Stalker portals remain separate under
**Live TV sources & filters → Choose source**. Groups, search, favorites,
recent channels, the programme guide and channel controls use the existing
Live TV screen. Use **Reload channels** to refresh the public list.

Every time the switch is turned on, it shows:

> These are free IPTV channels provided by third parties. Nuvio Max'd does not
> own, host or control them in any way. Availability and playback quality vary
> (YMMV). Please do not report dead channels or buffering to the Nuvio Max'd
> developer. This optional feature is intended to help users who do not want
> to pay for an IPTV subscription.

The controls and disclaimer are translated into all eight menu languages.
Turning the switch off removes the free source from selection and cancels
its pending requests; saved preferences remain available for re-enabling.
Profile configuration backups retain the choice, countries and disclaimer
acceptance. Personal source credentials are never sent to the public catalogue.

## Sources and limits

An independently written client reads the public per-country catalogue and
channel schedules from [jyotiprakash101/nuvio-livetv](https://github.com/jyotiprakash101/nuvio-livetv/tree/data).
This is the catalogue used by [Horizon Home Kit](https://github.com/jyotiprakash101/horizon-home-kit).
It combines iptv-org, Free-TV, AdeelWajid's validated list, dearbulut's online
list, and Samsung TV Plus, Pluto TV and Roku listings via Matt Huisman's
i.mjh.nz. Those services remain independent of Nuvio Max'd. We do not run or
mirror their video servers, bypass subscriptions, or certify every stream.
Availability, ads and regional restrictions depend on the provider.

Selected countries are combined into one list. Duplicate station IDs appear
once, keeping the first selected country's stream and guide. Country groups
(such as US and UK) let you filter the combined list. Your old single-country
choice carries over automatically.

Up to 256 countries may be selected. Each catalogue is limited to 8 MiB and
10,000 records; a combined load allows at most 32 MiB, 100,000 channels and
90 seconds of network time. Slow or failed countries do not discard successful
ones. If some countries fail or a limit is reached, a message recommends
reloading or choosing fewer countries. Country metadata is limited to 256 KiB. Explicitly adult
records, malformed entries, unsafe schemes/credentials/headers, duplicate IDs
and links explicitly marked failed by the feed are omitted. Other feed health
labels are not presented as guarantees. The first usable listed stream is used.
Programme data is fetched only around the visible channels, with two concurrent
requests, a 1 MiB per-channel response limit and bounded programme/cache counts.
No private IPTV credentials or playback history are uploaded to this service.

Tests cover the seven provider labels, malformed/bounded data, cancellation,
disclaimer acceptance/cancel, save rollback, preservation of 16 personal sources,
encrypted backup consent, multiple-country persistence and legacy migration,
Apply/Cancel and empty selection, correct per-country guides, duplicate station
identity, partial failures, aggregate limits, stale replies and profile changes. Host UI previews
check the full disclaimer fits without truncation in all eight languages.
Physical PS5 playback and individual external channels still require testing.
