# USB playlists — issue #20 development build

Live TV → Sources & filters → Import playlist from USB. Select an M3U/M3U8
channel list from /mnt/usb0 through /mnt/usb7. Folder scanning visits at most
8192 entries, four nested directory levels and 512 matching files. At most 16
imported sources are kept per profile. Existing 128 MiB / 100,000-channel import
limits apply. Symlinks and non-regular files are rejected.

The validated playlist is copied privately into the active profile, mode 0600,
using an atomic replacement. Remove the USB after import; Reload channels and
app restarts use the saved copy. Select the source then Replace current USB
playlist to update it from a new file. Invalid imports keep the previous copy.
Removing an imported source also removes its saved playlist copy.

HTTP/HTTPS channel addresses are required. Local media paths and relative
channel paths are unsupported. M3U8 channel lists are accepted; single-stream
HLS media/master manifests are rejected with existing parser guidance. Guide
URLs must be absolute HTTP(S), or configured separately in Live TV settings.
USB import does not make online streams available offline.

Host tests exercise nested scanning, symlinks/traversal, oversize files, HLS
rejection, atomic replacement, cancellation, private permissions, profile
isolation and reload after USB removal. Existing large-playlist regressions
also pass. Physical PS5 USB mount access and unplug/cancel/restart behavior
remain pending. Build 1.0.0.13 is unpublished.
