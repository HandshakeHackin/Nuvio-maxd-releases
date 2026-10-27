# Max’d 1.0.0.2 test candidate

Title ID: PPSA99288. Content version: 06.000.002. This build is unpublished.

Close the app before replacing its existing installer. Use PPSA99288.ffpfsc for
a ShadowMount+ image installation, or PPSA99288.zip for an existing folder-title
installation. Keep existing profile/settings data. Do not install both layouts
at once. ShadowMount+ itself is a separate console update, not in this package.

- Confirm Settings lists EVO Player 0.13.0; Native remains the default.
- Compare ordinary 1080p and 4K sources with the prior build. Check pans, colour,
  subtitles, pause/resume and seeking. Try Native, EVO and VLC separately.
- Try a known VP9 Profile 2 source. Hardware acceptance depends on the console;
  software fallback is expected if rejected. HDR output still needs console testing.
- Import a VOD-capable Xtream service in Live TV. IPTV Library should appear as
  a separate sidebar tab. Live-only accounts should keep it hidden.
- Try movie playback and a show's seasons/episodes. Resume stays profile-local;
  ordinary addon catalogs, history and tracker sources should not be mixed in.
- An offline provider should retain its saved catalog. HTTP 5xx should explain a
  possible provider outage; rejected login, unavailable title and connection
  failures should have their own messages.
- Check channels belonging to several provider categories in each group.
- Check the TV remote with HDMI Device Link enabled. Not every TV sends every key.
- If anything fails, use Settings playback diagnostics and provide the version,
  selected player, source codec/resolution and logs without URLs, passwords or tokens.
