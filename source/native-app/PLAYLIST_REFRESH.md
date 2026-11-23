# Automatic channel updates

Open **Settings → Live TV → Channel list auto-refresh** and choose **Off**, **Every
hour**, **Every 3 hours**, **Every 6 hours**, **Every 12 hours** or **Every 24 hours**.
The default is **six hours**, and each profile saves its own choice.

The setting refreshes the channel list you have loaded: free channels, your own
M3U/Xtream/Stalker playlist, or all enabled sources in the unified guide. Imported
USB playlists reload their private imported copy; this does not rescan a USB drive
or import a changed external file. Other sources load current data when selected.
IPTV movie/series categories keep their existing refresh-on-open behavior.

The timer runs only while Max'd is open, after the first channel-list load. It
waits during playback, multiview, loading, typing, sign-in, backups and open menus.
If playback or a menu opens during an automatic download, the download is retired
and its result cannot replace the list. An explicit manual reload remains available
with auto-refresh Off. Closing the app does not leave a background download service;
a fresh session loads the source when you open it.

Failed refreshes keep both the previous channel list and guide. A partially failed
multi-country or unified download also keeps the last complete view. Automatic
success does not interrupt you with a toast. Failures appear in the setting's help
and retry with a bounded delay: 5, 10, 20, 40, then at most 60 minutes, never during
playback or with Off selected. Manual reloads can still show available partial data.

Guide/programme updates keep their separate existing schedule. Favorites, renamed
or hidden channels, groups and per-source stream headers remain in the profile.
No search queries, provider credentials or private URLs are logged by the scheduler.
The existing source timeouts, channel/byte limits and private atomic saves remain
in force. Selecting a new source or profile invalidates older download results.

The free-channel upstream is currently scheduled to rebuild its online catalogue
every three hours. Its availability and update schedule are controlled by its
maintainers. Max'd fetches the available data; this setting does not repair dead
channels or make third-party streams reliable.

## Verification and PS5 checklist

Host tests exercise all intervals with simulated elapsed time, real private-source
HTTP requests, public-catalogue responses, no overlap, cancellation before/after a
response, failed and partial refresh retention, guide preservation, retries, Off
plus manual reload, durable private settings, backup validation and profile races.

On PS5, verify the chosen interval survives a restart and differs across profiles.
Compare free and personal sources and the unified view. Start playback or open a
menu during a scheduled refresh; it must not interrupt playback or replace a menu's
channel. Disconnect the network before a refresh and check the previous channels
and guide remain. Confirm Off still allows a manual reload. New scheduling behavior
still needs physical console confirmation.
