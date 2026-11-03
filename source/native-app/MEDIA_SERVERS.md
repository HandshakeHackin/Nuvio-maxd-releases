# Jellyfin, Emby and Plex — experimental support

Jellyfin and Emby were delivered in the 1.0.0.6 tester. Plex is staged on `builds/v1.0.0.7` for the next build; it has not been published. “Emmy” in the feature request is treated as Emby.

## Connect and watch

1. Select your Nuvio profile, then open **Settings → Account & Services → Media servers (experimental)**.
2. For Jellyfin/Emby, choose **Connect Jellyfin** or **Connect Emby**. Enter the server's reachable HTTP/HTTPS address (including its port and any reverse-proxy base path), username and password. Password input is masked and disables keyboard learning and resume.
   For Plex, choose **Connect Plex**. Scan the QR code or enter the displayed code at **plex.tv/link**, approve Max’d, then choose an accessible Plex server. No Plex password is entered on the PS5. The selected server identity is checked before saving.
3. A **Media Servers** sidebar tab appears after successful sign-in. It stays hidden for profiles without a saved connection. Up to eight connections can be saved per profile.
4. Browse a movie library, or open a series to browse its episodes in season/episode order. Triangle searches the selected server. Circle returns to the previous browse location or cancels pending playback negotiation.
5. Select a video for **Play / resume**, **Play from beginning**, **Compatible playback (server HLS)**, or an available file version. The selected existing Native/EVO/VLC engine handles playback.
6. Options → select a connection provides **Continue watching**, library browsing, search, refresh, **Next page / Previous page**, reconnect and confirmed removal. Pages contain at most 100 items.

Direct playback is preferred. Compatible playback asks the server for H.264/AAC HLS; the server must permit and support transcoding. A playback failure explains how to try compatible playback or reconnect. This does not add Android Media3 or change the PS5 decoder/HDR limitations.

## Library and progress behavior

Server-owned titles have separate identities. They do not replace addon source discovery, the Nuvio library, IPTV Library, Trakt/SIMKL preferences or the normal Continue Watching history. Server watch positions and watched markers populate the dedicated page, and the server's resume list supplies **Continue watching** there. This first version does not inject server matches into addon stream lists or import server collections into Nuvio's backend.

Playback sends server start, progress, pause/resume, seek and stop reports. A single worker keeps reports ordered, coalesces pending progress and caps queued work at 16 jobs. Failures do not interrupt video; they produce a generic diagnostic message. Reporting is best effort, not an offline sync queue; abrupt shutdown or server unavailability can lose the final update. Refresh Continue watching to read the latest server state.

Embedded audio/subtitle tracks use the existing player controls. External server subtitle delivery, server music libraries, Jellyfin Quick Connect, automatic server discovery and Nuvio cloud synchronization of connection credentials are outside this experimental version.

## Credentials and isolation

- Jellyfin/Emby save access tokens, server identity and user identity; passwords are not persisted. Plex additionally saves a per-connection Ed25519 device key, key ID and account token so expiring JWT credentials can be renewed. Account tokens go only to Plex’s HTTPS API; server-specific tokens authorize playback and server API requests. Renewed credentials are saved on the next ten-second UI persistence check. Connections use the existing atomic, mode-0600 private-file writer in the active profile's `media-servers.json`.
- Public sign-in and authenticated API calls have response/time bounds and do not follow redirects. HTTPS certificate verification remains enabled. Redirected addresses need their final reachable URL entered in Settings.
- Playback headers are scoped to the server origin and the internal scope marker is removed before transport. Foreign HLS origins do not receive server authorization; authenticated media requests do not follow redirects. Native/EVO scoped HLS rejects local-file input and writes.
- Negotiated playback URLs remove API-token query parameters, including encoded parameter names. Artwork uses opaque connection/item cache keys and authenticated worker requests. Reconnect creates a fresh connection identity so a different server/user cannot reuse an old artwork identity.
- FFmpeg diagnostics redact HTTP(S) URLs, including URLs embedded by a server in an HLS playlist.
- Profile generations and cancellation guards suppress stale browse/auth/playback results. Profile changes stop active server video and clear the old account state. Addon/plugin inheritance does not inherit media-server credentials.

## Verification and acceptance

Automated checks cover Jellyfin and Emby API shapes, authentication, reverse-proxy paths, bounded paging/responses, search, episodes, resume, media versions, direct/HLS negotiation, progress, denied access, invalid JSON, cancellation, redirect rejection and credential scoping. Production Session tests exercise connection callbacks, private persistence, reconnect, browsing, cancellation, profile changes, playback dispatch and removal. Production sidebar tests cover both optional tabs, and real FFmpeg tests decode authenticated HLS audio/video with extensionless segments and verify URL-token redaction.

See `MEDIA_SERVERS_VALIDATION.json` for the completed checks and source hashes. Physical PS5 playback and interoperability with real Jellyfin/Emby/Plex server installations remain pending. Before public rollout, test all three server types, a reverse proxy, seeking/resume/completion, embedded subtitles, all player choices and rejected/expired credentials.

## API reference

Independent C++ implementation against the MediaBrowser API, reviewed alongside [NuvioTV's media-server implementation](https://github.com/NuvioMedia/NuvioTV/tree/f362dfeea1d241a7d57bfb690c4bc24789839e40/app/src/main/java/com/nuvio/tv/data/mediaserver) at commit `f362dfeea1d241a7d57bfb690c4bc24789839e40` (GPL-3.0). Android player/UI code is not embedded. Password option values were checked against the public [IME ABI definitions](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/ime/ime_common.h).

## Plex scope and validation

Plex movie/show sections, episodes, search, resume positions, file versions, posters and timeline reports are mapped into the existing Media Servers page. Search is limited to 100 results; library pages contain at most 100 items. Compatible playback requests H.264/AAC HLS at up to 1080p, subject to Plex server policy and transcoding support. Direct playback retains the original file. Remote resources require HTTPS; LAN HTTP resources advertised by Plex are supported. TLS verification stays enabled, including plex.direct certificates.

Plex Home managed-user/PIN switching, multi-part files, music, Live TV/DVR, Plex’s online catalog and server subtitle download APIs are outside this first version. A connection uses the account approved during linking. Separate Nuvio profiles can link different Plex accounts. Disconnect removes the local saved connection; revoke Max’d in Plex’s authorized-device settings if account-side revocation is desired.

`tests/plex.sh` checks signed device pairing (verifying Ed25519 signatures against the registered public key), resource filtering, identity matching, browse/search/paging, direct/compatible playback, denied transcoding, timeline reporting, token renewal, credential scoping and cancellation. `tests/media_session.sh` also drives the production Plex modal/server-picker callbacks and checks private persistence, stale-profile suppression and renewal persistence. These fixtures do not replace real-server/PS5 acceptance. See `PLEX_VALIDATION.json` for this change’s results; the older media-server receipt remains historical.

Implemented against [Plex’s official API and device-JWT authentication documentation](https://developer.plex.tv/pms/), reviewed 2026-11-03. No Plex application or player binary is bundled.
