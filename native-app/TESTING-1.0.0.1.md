# Max’d 1.0.0.1 local test

Install the included PPSA99288.ffpfsc (or extract PPSA99288.zip for your usual folder-based installation). Use the same installation method that works for your current build. Fully close Max’d before replacing app files. Preserve your existing user-data/settings folder; do not copy old app binaries over the new package. Keep your 1.0.0 installer for rollback.

Confirm Settings → App updates reports 1.0.0.1. This test release is available on GitHub; Homebrew Store availability follows catalog approval and deployment.

1. Import the previously failing large M3U or Xtream Live TV source. Check channel counts, browsing, cancellation, and a normal full-screen channel. Only live channels are imported.
2. Open Live TV’s sources/filters menu → Four-screen TV (experimental). Choose four different SDR channels. Start with SD feeds and a provider allowing four simultaneous connections. D-pad focuses; Cross listens to that tile; Square replaces it; Triangle opens it full-screen; Circle exits. Check that sound comes from only one tile and that normal playback works afterward.
3. In Settings → Audio, try volume/boost and audio sync. Defaults are 100% and 0 ms. Positive audio delay makes sound later relative to the picture. Boost uses PCM and can clip loud peaks. During playback, open the Audio/Subtitles menu: Left/Right chooses a column; L2/R2 changes sync; L1/R1 changes Audio volume; Triangle resets that column.
4. In Settings → Subtitles, try a delay. Positive subtitle delay shows captions later. Confirm defaults persist after restarting and switching streams.
5. Check ordinary Native, EVO, and VLC playback, pause/resume, seek, track switching, and exit.

Four-screen mode uses software decoding and small preview surfaces, not four full-resolution hardware decoders. SDR sources up to 1080p are accepted, but four HD feeds may be too demanding. 4K/HDR sources are rejected in the grid. Console/GPU performance is unverified.

If a problem occurs, export the playback logs from Settings immediately afterward and report the player, source type, tile count, and steps. Remove stream URLs, passwords, and account tokens before sharing.
