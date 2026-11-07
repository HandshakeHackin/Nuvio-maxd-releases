# Nuvio Max’d 1.0.0.13 experimental — USB playlist test

- Add Import playlist from USB under Live TV → Sources & filters. Scan /mnt/usb0 through /mnt/usb7 for M3U/M3U8 channel lists in up to four nested folder levels.
- Save a private copy per profile, so the playlist reloads after USB removal and app restart. Online channels still require network access.
- Replace the selected USB playlist without losing the previous copy if validation fails; removing a USB source removes its saved copy.
- Retain 128 MiB / 100,000-channel limits, cap scanning at 8192 entries / 512 files and imported sources at 16 per profile. Reject symlinks, local media paths and single-stream HLS manifests.

This prerelease is for issue #20 console testing. Compilation and host/package checks are recorded with the download; physical PS5 USB mount access and unplug/cancel/restart behavior still need confirmation. Keep the previous installer and saved user data. Fully close the app before installing; do not overwrite a mounted image. Use PPSA99288.zip for a folder install or NuvioMaxd-1.0.0.13-native.ffpfsc for an image install. Content version: 07.000.013.
