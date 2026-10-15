# Local audio compatibility test: 0.5.25

This build is for listening tests, not a public release. It includes the 0.5.24
controller lightbar. Native video and HDR rendering are unchanged.

- PCM prefers 48 kHz signed 16-bit samples. Surround requests try the eight-channel
  S16 port, then stereo S16, then float stereo only if both S16 ports fail.
- FFmpeg stereo conversion explicitly mixes center and surrounds at -3 dB and
  LFE at 0.5, with matrix normalization for headroom. Genuine stereo stays at
  unity gain with its polarity preserved. S16 quantizes higher-bit-depth sources
  to 16-bit output; this compatibility mode is not bit-perfect 24-bit audio. No loudness leveling or volume boost.
- New settings default to stereo; existing saved surround settings are retained.
  Auto honors Surround off by choosing PCM; HDMI capability probing is used only
  when Surround is on. Forced Bitstream remains an explicit experimental choice.
- PCM's media clock subtracts two 256-frame console output grains (10.67 ms).
  This is an estimate of device buffering, not a measured TV/soundbar delay.
  Bitstream timing is unchanged.
- Logs record the decoded layout, sample format, output format, five-second
  output peak/RMS levels, and queue duration. They contain no sample audio.

Test Native + PCM + Surround off first, with the same stereo and 5.1 scenes that
had weak dialogue/music on 0.5.24. Compare levels, dialogue, music and lip sync.
Then test surround PCM only on a compatible HDMI surround setup. Auto/bitstream
are still experimental; HDMI capabilities do not identify an optical connection.

EVO's green screen and resident-decoder memory failure are separate issues and
are not claimed fixed by this audio build. Use Native during this comparison.

References: Stremio-Plus 64d1bfb (S16), Mortalfin's jf_player.cpp and
ProsperoPlayer 282b449 (explicit stereo matrix), ProsperoLight 136ac41
(S16 ports), VLC d46be6a (console buffering estimate). These informed the
independent Nuvio adaptation; original component licenses remain bundled.
