# Unified TV guide

Open **Settings → Live TV → Unified TV guide**. You can also choose
**All sources — unified guide** from **Live TV sources & filters**.
Your personal M3U, Xtream, Stalker and imported USB channels appear together
with Free Live TV when you have enabled it and accepted its disclaimer.
The guide uses your existing channel-tile or time-grid layout.

Each channel shows its source. Existing favorites, recent channels, names and
hidden-channel choices are shared with that source's individual view. Search,
groups, channel controls, Sports Hub matches and four-screen TV use the combined
list. The selected view and remembered group belong to the current profile and
are included in configuration backups. Choose an individual source to browse
or remove it separately. IPTV movies and series remain in IPTV Library.

Exact repeat entries from one source are omitted. Similarly named channels
from different providers remain separate, with source labels. **Backup channels**
can offer matching alternatives across the loaded sources. You choose when to
switch; each alternative keeps its own URL and request headers. Provider-local
channel and EPG IDs are never treated as global identities.

Programme data uses each channel's original source: XMLTV, Xtream, Stalker or
the supplied free-channel guide. XMLTV overrides and clock corrections remain
per source. From the combined view, guide settings first ask which source to
edit. Guide errors do not stop channel playback.

Loading allows up to 16 personal sources plus the enabled free source, 100,000
channels and 32 MiB of retained channel data. A combined load has a 120-second
budget, with 30 seconds per source; use an individual source view for a slower
large import. Available results survive failures in other sources. Messages
explain partial loads without exposing URLs or credentials. Cancelling, changing
profiles or turning off Free Live TV cancels obsolete requests and guide results.

Programme requests share a limit of two in flight and a 256-channel cache.
XMLTV documents share one 32 MiB retained cache across providers, with LRU
eviction. Existing per-response parser, programme and decompression limits
remain in effect. Source credentials stay in the selected profile and go only
to the corresponding provider; they are never sent to the public free catalogue.

Host tests cover source/EPG ID collisions, per-source clocks and credentials,
XMLTV reuse/eviction, request and memory bounds, partial failures, cancellation,
Free TV consent, preserved channel controls and profile boundaries. The native
PS5 package is built separately; physical PS5 playback still requires testing.
