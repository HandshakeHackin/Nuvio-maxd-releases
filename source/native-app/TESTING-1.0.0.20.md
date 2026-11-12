# 1.0.0.20 test checklist

The three new integrations are unconfirmed on physical PS5 hardware and live
provider accounts. Use a test profile and a copy of your configuration first.
Keep credentials, MAC addresses, playlist contents and private URLs out of reports.

## Encrypted configuration backup and sync

- Choose a profile and stop playback. In Settings → Configuration backup and sync,
  create a local backup and confirm its separate passphrase. Reopen the app and
  restore it after changing a harmless preference. Check that settings, sources
  and imported USB playlists return; watched history and resume stay unchanged.
- Check cancellation, a wrong passphrase, a changed/corrupted file and a backup
  belonging to a different account/profile. Existing settings must remain intact.
- Copy only the encrypted `configuration-backup.nmb` between installations of the
  same account/profile using your normal console file manager. Do not copy tokens
  or entire profile directories. Backups have a 16 MiB plaintext limit.
- With a suitable Google OAuth TV application registration, configure its client
  ID/secret and approve `drive.file` access using the phone code. Sync is manual.
  Confirm upload verification, restore review and equal-version guidance.
- On two devices, edit settings independently before syncing. Confirm that the
  conflict menu asks which version to keep. An update made after a version was
  reviewed must not be silently overwritten. A missing revision tag must prevent
  an unconditional update. Disconnect should retain cloud backups.
- A failed Google registration/scope request should explain the connection issue;
  local encrypted backups must still work. No Google OAuth registration is bundled.

## TVDB episode ordering

- Supply your own appropriate TVDB API access in Settings and test it. Pick a
  series with known aired/DVD or absolute ordering; use Options → Episode order.
- Check displayed seasons/episodes and source searches against the selected order.
  Next episode should follow that order. Reopen the series/app to check persistence.
- Mark a displayed season watched and inspect history on another Nuvio/tracker
  device. Canonical episode identities and previous resume positions must survive
  order changes. Choose Provider order to return to the default.
- Verify that wrong keys, network failures, ambiguous IDs, changed metadata and
  split/combined episodes retain provider order with a message. Do not interpret
  an alternate order as evidence that a source offers those files.
- Switch profiles or titles during a lookup; a late result must not apply to the
  new profile/title. Removing credentials must affect only the selected profile.

## Stalker / Ministra

- In Live TV → Options add a portal with the MAC assigned by your provider and,
  if required, its username/password and serial. Use the final portal URL.
- Reload channels, browse groups, play several channels, check favorites/recent
  and the guide. Try a signed link hosted on a different media server.
- Check IPTV Library availability, movie/show categories, seasons/episodes,
  playback, private cache reload and local resume. These must stay outside addon
  catalogs, normal Continue Watching and account/tracker scrobbling.
- Test account rejection, provider outage, empty/invalid/repeated pages and
  cancellation/profile switching. Partial failed imports must retain saved data.
- If your provider permits four connections, test four-screen playback, audio
  switching, replacement, full-screen selection and leaving the view.

## Existing behavior

- Retest USB import with the drive unplugged after import, profile isolation,
  menu language switching, playback/resume, title watched/library actions,
  subtitles, audio switching and remote/controller navigation.
- Report app version, console model/firmware, installation format, which step
  failed and the redacted error text. Never attach a backup or profile folder.
