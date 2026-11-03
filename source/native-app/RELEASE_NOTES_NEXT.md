# Staged for the next Max’d build

- Issue #14 password-keyboard test fix: secret fields now select Basic Latin input while retaining masking and the existing learning/resume protections. Intended to address the firmware 7.00 keyboard-open error; reporter confirmation pending.
- Maintenance updates: FFmpeg 7.0.3, Expat 2.10.0, libpng 1.6.59, FreeType 2.15.0, HarfBuzz 15.0.0 and FriBidi 1.0.18, plus two additional libass drawing-input fixes.
- Native/EVO HEVC compatibility: discard Dolby Vision enhancement-layer pictures while keeping the HDR10-compatible base layer. VLC already applies this filtering; no new Dolby Vision output mode is added.
- Experimental Plex support: link your account with a QR/code at plex.tv/link, choose a server, browse movies/shows, search, resume and report watch progress. Includes direct playback, compatible HLS and automatic credential renewal. Saved per Nuvio profile alongside Jellyfin/Emby; real Plex-server/PS5 testing is still required.
- Experimental Jellyfin and Emby connections: browse movies and episodes in a separate Media Servers tab, search, resume server playback and report watch progress. Saved separately for each Nuvio profile, with direct playback and a compatible server-HLS option.
- Optional Night Mode lifts quiet dialogue while limiting loud peaks; works with PCM on Native, EVO and VLC. Off by default.
- Choose how much surround bass reaches stereo in Native/EVO: Off, 50% (existing default), or 100%.
- Correct each IPTV provider's programme-guide clock by up to 12 hours in either direction.
- Opening, recap and credits chapter labels can provide skip buttons while preserving post-credit scenes.
- Prefer trailers in your chosen language, with English fallback.
- Optional Native/EVO subtitle auto-sync aligns addon captions to matching embedded text timings; uncertain or unsupported sources keep manual timing. Off by default.
- Subtitle rendering no longer holds the media reader behind the libass render lock; queued work is bounded and cleared on seeks.
- Harden subtitle input parsing with three upstream libass fixes.

New features require console/provider testing. Guide clock correction is under Live TV → Options; audio, chapter, trailer and subtitle controls are searchable in Settings. Existing tracking, progress-saving and artwork fixes are retained.

Media servers: Settings → Account & Services → Media servers (experimental). The sidebar tab appears after connecting. Real PS5/server acceptance is still required; see [media-server setup and limitations](MEDIA_SERVERS.md).
