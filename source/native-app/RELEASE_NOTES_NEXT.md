# Unreleased Sports Hub

- Add an optional per-profile Sports Hub, disabled by default, with a dedicated Settings section. Enable/disable, choose sports and competitions, follow teams/leagues/names, choose a coverage region, hide scores, restrict to channel candidates and adjust the display time offset.
- Adapt ReSeq’s sport → league → team selection flow to the native controller UI. Retain search and focus while following multiple teams. Disabling retains selections and cancels sports work.
- Browse schedules, My Sports, started/upcoming events and results. Use the shared public ReSeq catalogue and limited focused-event score requests; stale snapshots never present live scores as current. Date-only fixtures show time TBD.
- Match programme titles and broadcasters against the selected Live TV source, label uncertain broadcaster candidates, and offer manual channel search. Playback still requires the user’s own configured source.
- Keep sports matching bounded and index channel names for large playlists. Include catalogue provenance, GPL notice and public-data attribution.

This is an unpublished native adaptation of Nuvio-ReSeq at d46b29a. Public sports coverage varies; physical PS5 and provider testing remain pending.

# Unreleased source selection

- Add per-profile source preferences for minimum resolution, maximum advertised file size, preferred language and HDR. Unknown metadata stays eligible.
- Check supplied filenames for archives/non-video files, likely samples and clearly different episodes. Handle episode ranges, season packs and mapped anime numbering conservatively. Cinema-recording labels receive a warning, with optional exclusion.
- Explain source ranking and warnings on stream cards. Open Source options beside the addon filters to play the top-ranked source, adjust preferences or show all enabled sources for the current search.
- Apply the same filters to next-episode source selection. Never silently relax user filters or enable torrents through Show all.

Inspired by MKCinema’s source-selection features; implemented in native C++ with no new service or dependency. Source metadata remains unverified; physical PS5 testing is pending.

# Unreleased torrent integration

- Show the selected filename separately from Resume, Start over and Choose another file controls.
- Show launch stages, transfer speed, verified data ready ahead and actual buffered playback seconds.
- Recover with retry, restart or alternate file/source while preserving saved progress; reject stale callbacks.
- Group optional torrent controls, allocated cache usage, safe cleanup and Web Drop status in settings.
- Export a privacy-filtered diagnostic summary as on-screen text, a plain-text QR for phone copying, or a local text file.

- Resume the most recently watched file in a torrent, or restart only that file. Keep other files’ progress and ignore stale profile callbacks.
- Resolve the actual selected file before applying/saving torrent progress, including automatic magnet-file selection. Save progress for streams with unknown duration.
- Use short head-only probing for MKV/TS/M2TS, while preserving MP4 tail-metadata access and indexed resume behavior.
- Report buffering when actual torrent data has arrived, rather than inferring it from peer count alone.
- Interrupt blocked torrent reads for new seeks. Bound index-search reads, roll failed seeks back to a confirmed demux position, and report unrecoverable failures without reopen loops. Decoder/audio errors remain visible.

Based on the latest torrent-edition source at c8837a9. This is an unpublished development candidate based on official 1.0.0.11; physical PS5/swarm testing remains pending.

# Included in 1.0.0.11

- Dependency security: replace the legacy Cheerio fork with maintained browser-compatible selectors, removing the known nth-check denial-of-service advisory while retaining provider module aliases.
- Maintenance: HarfBuzz 15.1.0, WebP 1.6.0 and libxml2 2.15.4, including PS5-compatible secure XML randomization. FFmpeg and graphics release lines stay pinned.
- Add weekly dependency/security checks and reviewable update proposals; activation requires merging the workflow/configuration into the default branch.

- Issue #14 follow-up: authenticated Media Servers posters now reach the artwork worker. Connection loading uses bounded descriptor-based reads; saves are read back and verified, with private diagnostics for load/rejection/save failures. PS5 restart confirmation remains pending.
- Issue #14 password-keyboard test fix: secret fields now select Basic Latin input while retaining masking and the existing learning/resume protections. Intended to address the firmware 7.00 keyboard-open error; confirmed by the reporter on firmware 7.00.
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
