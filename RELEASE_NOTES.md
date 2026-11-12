# Nuvio Max'd 1.0.0.20 — configuration backup, TVDB ordering and portals

- Encrypted per-profile configuration backup/restore, including private imported USB playlists. AES-256-GCM with PBKDF2-HMAC-SHA256; rollback recovery; no account sessions or playback history in backups.
- Manual Google Drive configuration sync with three-way conflict review, conditional updates and upload verification. Google OAuth TV application setup is required; no registration credentials are bundled.
- Per-series TVDB episode order selection using your own API key/PIN. Source searches and next-episode navigation follow the selected order; watched/history/resume identities remain canonical. Ambiguous split/combined mappings retain provider order.
- Stalker/Ministra portal sources with provider-assigned MAC and optional login/serial. Live channels, signed playback links, guide and available movies/series use bounded, cancellable requests. Portal credentials are never forwarded to media CDNs.
- New menu entries translated into all eight supported languages; advanced help may appear in English.

These features are implemented and regression-tested, but live account/provider behavior and physical PS5 operation are **unconfirmed**. Please use the testing checklist before relying on them. Existing USB diagnostics/import validation, profile isolation and private atomic copies remain in place.

No playlists, media, account access, TVDB API subscription or Google OAuth app registration are supplied. The app remains independent of its metadata, account and content providers. Metadata: TheTVDB.com and TMDB, without endorsement.


See TESTING.txt for the console/account/provider checklist. Matching source and verification results accompany this release.
