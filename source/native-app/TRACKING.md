# Tracking accounts and sources (0.5.31 test build)

Open **Settings → Account & Services → Tracking accounts** after signing in to
Nuvio and selecting a profile. Trakt, SIMKL and MDBList have independent
connection entries. Library Source and Watch Progress are also available as
separate settings in Account & Services.

- **Library Source** chooses which source Nuvio reads for your Library.
  Nuvio Library is the default. Trakt and MDBList show the account's watchlist;
  SIMKL shows its tracked movies and shows. This selector does not import or
  overwrite Nuvio library membership. Custom tracker lists are not included yet.
- **Watch Progress** chooses which progress source powers resume and Continue
  Watching. Nuvio Sync is the default. When another tracker is selected, new
  playback progress and watched marks go to that tracker, rather than being
  broadcast to every connected service.
- Connecting a tracker does not automatically change either source. Only verified,
  connected trackers appear in the selectors. Stop playback before changing
  connections or sources.
- Credentials, preferences, retry queues and resume caches belong to the selected
  Nuvio account/profile. Disconnect here affects this PS5 only, returns selected
  sources to Nuvio and clears that tracker's pending events; it does not revoke
  the connection on another device.

## Connection setup

**MDBList:** choose Connect with API key and enter the key from
https://mdblist.com/preferences/. The app validates it with `/user` before saving.

**Trakt:** choose Connect with phone / QR code, open
https://auth.trakt.tv/activate on your phone and approve the displayed code with
your regular Trakt account. Builds include the owner's public Max'd Client ID;
users do not need a developer account or a Client Secret. TV pairing uses the
device-code flow, including secret-free token exchange and refresh rotation.
Approving a connection does not change your selected library/progress sources.

**SIMKL:** phone pairing still requires an owner-registered SIMKL client ID.
Until one is included, use a compatible synced connection or the advanced
application client ID setting. Regular users should not have to register apps.

The default public registrations are in `app/tracker-applications.json`.
`NUVIO_TRACKER_APPLICATIONS` can override that file at build time. The validator
accepts only Trakt/SIMKL `client_id` fields and rejects secrets, account tokens
and API keys. Existing connected profiles retain their original application
identity; disconnect restores the bundled registration for a new pairing.

The owner registered the required HTTPS callback at
https://handshakehackin.github.io/Nuvio-maxd-releases/trakt/callback/.
Device-code pairing does not redirect through that page or use a redirect URI
when refreshing tokens. The static page is reserved for registration; it does
not implement browser authorization or forward authorization codes.

Import connection from Nuvio Sync is a read-only, explicit copy of a compatible
credential record from the selected Nuvio profile. MDBList uses the synced
`mdblist` / `api_key` record. Trakt/SIMKL import requires a compatible token and
application ID record; not all Nuvio clients sync their OAuth credentials. If none
is available, connect locally. Source choices remain local and default to Nuvio.

## Failure handling and limits

A failed library/progress read uses Nuvio for that read and displays a fallback
message. A rejected tracker token requires reconnecting. A transient outage does
not erase the preferred source; retry on the next library refresh. Progress writes
remain on this PS5 for their original selected tracker and retry later; they are
not silently uploaded to another tracker or account. The UI retains recent local
resume positions for that selected source while an update is unsent.

Only titles with supported external IDs and explicit episode coordinates can be
sent safely. Ambiguous/custom IDs stay local rather than being matched by title.
SIMKL anime without reliable TVDB season/episode coordinates also stays local.
Trakt progress below 1% stays local until it meets the service minimum.
Removing a Continue Watching entry remotely is supported here for MDBList; for
Trakt/SIMKL remove it on their service. Removal never marks the item watched.

Requests use fixed HTTPS API origins and never redirect credential-bearing
requests. Connection storage has owner-only permissions. Do not share
`tracking.json`, profile folders or unredacted backups; they contain credentials.
The UI status and error messages omit tokens, response bodies and request URLs.

Official API references checked for this implementation:
- https://trakt.docs.apiary.io/
- https://simkl.docs.apiary.io/
- https://api.mdblist.com/docs/ (OpenAPI `/schema/`)

## Testing

Automated fixtures exercise production tracker code, authentication contracts,
pagination, resume conversion, selected-service writes, offline retries, secret
redaction, permissions and profile isolation. These use fake accounts and do not
prove live tracker authorization or PS5 behavior. Test real pairing, expired-token
refresh, watchlist loading, cross-device resume, completion, source fallback,
profile switching and restart on a console before a public release.
