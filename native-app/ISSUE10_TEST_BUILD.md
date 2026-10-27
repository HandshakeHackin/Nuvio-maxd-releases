# Max’d 1.0.0.1 — local test build for issue #10

- Large Live TV imports: up to 100,000 live channels and 128 MiB per channel list, with per-channel Xtream parsing. Authentication/category metadata keeps smaller bounds. Xtream VOD and series libraries are not imported.
- Experimental four-screen Live TV: select four different channels from the current page/filter. D-pad focuses a tile, Cross listens to it, Square replaces it, Triangle opens it full-screen, and Circle stops the grid. The old audio owner is released before another opens.
- Four-screen previews use independent software decoding, one PCM audio output, 640×360 maximum surfaces and bounded packet queues. Sources must be SDR and at most 1920×1088; SD feeds are recommended. The provider must permit four simultaneous connections. EVO/VLC and hardware decoding remain available for ordinary full-screen playback.
- Audio volume/boost: 50–300%, default 100%. Non-default gain uses PCM; encoded bitstream data is never amplified. Loud peaks can distort when boosted.
- Audio sync: ±2 seconds in 50 ms steps. Positive makes sound later relative to the picture; negative makes it earlier. Native/EVO adjust picture timing against the audio clock without changing watch progress; VLC uses its audio-delay API.
- Subtitle sync: ±10 seconds, adjustable in Settings or during playback. Positive shows subtitles later.
- Playback track menu: select Audio or Subtitles with Left/Right; L2/R2 adjusts that column’s sync. On Audio, L1/R1 changes volume/boost. Triangle resets the focused column. Active encoded bitstream must be changed to PCM before live boost adjustments.
- Defaults persist in Settings → Audio and Settings → Subtitles. A new stream uses those defaults.

## Test scope and limitations

Host tests cover PCM sample scaling/clipping, preference persistence, actual PCM output gain, sync without a progress reset, four independent preview players, muted-tile audio ownership, audio handover and stop cleanup. Preview tests decode local files; they do not prove four live provider connections on PS5. UI input boundary tests cover grid focus, replacement, full-screen and exit. PS5 compile checks cover the changed paths. Console/GPU multiview performance, subscription connection limits and Bluetooth latency need physical-console testing.

This is a local test candidate, not a public release or a store update. The public 1.0.0 release is unchanged.
