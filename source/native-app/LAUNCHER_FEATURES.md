# Start page and Live TV controls

These native PS5 additions were inspired by a review of
[Br0ck25/NuvioTVLauncher](https://github.com/Br0ck25/NuvioTVLauncher), GPL-3.0,
at commit `7867b98484a6fb2c4af4914abe494c825c2fc541` (2026-11-16).
The C++ implementation uses Max'd's existing player, menus and private profile
storage. Android launcher, accessibility, ADB and VPN code was not imported.
Credits also acknowledge the launcher's NuvioMedia and homelessbrian upstreams.

## What is included

- **Choose a start page:** Settings → App & Updates → Open Max'd on. Choose
  Home, Discover, Library, Live TV or Sports for this profile. Selection applies
  after opening the profile; it never starts playback. Disabled Sports falls
  back to Home. Unknown saved values fall back to Home.
- **Remember TV groups:** the last group chosen with L1 is saved separately for
  each profile and source. It returns after restarting or changing sources.
  If a refreshed playlist no longer contains that group, the list shows all groups.
- **Rename and hide channels:** press Options on a Live TV channel. Empty names
  restore the provider name. Favorites and guide IDs use the original identity.
  Search finds both the custom name and the provider name. Restore channels in
  Sources & filters → Hidden channels, individually or with Show all.
- **Choose backup copies:** Options → Backup channels on a channel, or Choose
  another source/Backup channels from the player's playback options. Copies
  come from the current loaded playlist, including hidden channels. Matching
  uses the guide ID or a conservative name comparison that removes common
  quality tags while retaining channel numbers, regions and +1 variants.
  Each choice keeps its own URL and headers. There is no automatic switching,
  cross-source downloading or fabricated stream URL.

All new menu text is translated into the existing eight languages. Start-page,
channel edits and remembered groups are included in encrypted configuration
backups. They do not write to another app's sync service or change account sessions.

## Bounds and verification

Channel customizations are keyed by the existing source/channel identity, never
by a custom label. Up to 1000 channels per profile can be customized across up
to 16 sources. Names are limited to 160 UTF-8 bytes and must be valid single-line
text. Restore dialogs list at most 200 channels; Show all covers the entire source.
Backup selection scans at most the existing 100,000-channel limit and presents
at most 32 distinct URL/header pairs. It requires explicit selection.

New edits roll back in memory when private atomic saving fails. Dialog and text
callbacks reject stale profile/source/list generations. Backup selection also
checks the active playback generation before stopping or replacing a stream.
Successful channel edits invalidate the visible guide window so newly exposed
channels are parsed from the cached bounded guide document on the next tick.
The startup destination is scheduled only after profile selection succeeds.
No new filesystem syscalls, device identifiers, services or dependencies are added.

`tests/launcher_features.sh` covers real production preference helpers, channel
list construction, menus, stale callbacks, save failures, header separation and
startup scheduling. `tests/features20.sh` covers encrypted round trips and rejects
malformed imported preferences before writes. Preview scenes use the production
dialog renderer. PS5 interaction still needs console verification.

## Other launcher features reviewed

| Feature | Max'd status |
|---|---|
| IPTV sources, guide grid and clock correction | Already supported |
| Sports, followed teams, score hiding, multiview | Already supported |
| Encrypted configuration backups | Already supported; extended with the new fields |
| Android Home replacement, installed-app launcher and ADB setup | Specific to Android |
| Calendar, TV recording and timeshift | Future work requiring scheduling/storage and playback design |
| Integrated VPN | Requires a separate PS5 networking implementation and testing |

The new version is prepared on its delivery branch; this document does not
claim a published release, store deployment or physical-console test result.
