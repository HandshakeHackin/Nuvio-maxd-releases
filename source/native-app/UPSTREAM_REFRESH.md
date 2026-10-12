# 0.5.16 upstream refresh

Native playback remains the default; EVO Player 0.12.0 remains optional.

- OpenGL SDK 1.0.1 includes its graphics-buffer lifetime fix. The archive and every SDK file are hash-verified before linking.
- Payload SDK 0.44 updates build tools and AGC stubs. PacBrew libraries remain at 40.2.
- MkPFS 1.1.0 includes compression compatibility fixes; the build enforces this version.
- Native presentation keeps due frames in timestamp order and catches up after a source frame interval is missed, avoiding discards for small scheduling jitter.
- Bounded background logging keeps kernel, stdout and storage writes off UI and playback producers. A full queue reports omitted messages instead of blocking playback.
- Long words and scripts without spaces wrap at UTF-8 code-point boundaries. Existing dynamic Unicode font fallback and bidirectional handling remain.
- Settings → Calmer backgrounds freezes decorative background motion and saves the preference locally. Video resolution, tone mapping and decoding stay unchanged.
- attribute3=0x40040 follows ProsperoLight 01.000.095 for older-firmware-compatible 120 Hz/VRR declarations. Native HDR acceptance still requires console testing.

Exact pins and adaptation scope appear in dependencies.json. This does not embed the entire Nuvio TV web UI, standalone ProsperoLight RTP player, or Stremio-Plus player. Their platform-specific loops cannot be substituted directly. The updater already retains a verified previous image for recovery. ShadowMount remains a separate console installation.

Validation covers Unicode wrapping, 24 fps at 60/120 Hz, late catch-up, pause/buffering holds, bounded logging, the existing host suite, PS5 linking, source provenance and image contents. Console comparison remains necessary for motion smoothness, firmware 11 HDR acceptance and HDMI signal readback. An accepted refresh request is not a measured HDMI refresh rate.
