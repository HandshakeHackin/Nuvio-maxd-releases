# Staged playback stability

These changes are staged after the 0.5.34.3 test build, alongside Live TV, anime timings and settings search. They are source changes, not a newly published package.

## VLC seeking and rebuffering

Patches 0027–0028 adapt [Stremio-Plus 0.5.18](https://github.com/LoZazaMastro/Stremio-Plus/commit/1475f4c66202325ff5bfde9e715e1dca4420a248), GPL-3.0:

- Restore the configured FFmpeg frame-discard policy after preroll even when hurry-up is disabled. A temporary seek policy must not remain active during normal playback.
- A late PCR/clock starts buffering without flushing queued access units or decoder references. Actual seeks still flush references. This avoids breaking inter-frame HEVC decoding while waiting for another random-access picture.
- Reject FFmpeg frames marked corrupt or carrying decode error flags. The last accepted picture remains available until a valid replacement arrives.

The existing Native/EVO recovery and FFmpeg ABI remain. The PS5 hardware decoder module's own picture validity checks remain separate. Rebuild the pinned libVLC sources before packaging; pre-existing library archives do not gain these fixes merely by updating app source.

## Native-resolution GPU resources

VideoSurface now detaches the output texture from its framebuffer and releases obsolete plane textures before allocating a different size or format. Failed output/plane allocations discard partial state rather than leaving reusable-looking dimensions or texture handles. Caller texture bindings are adjusted so deleted names are not restored. Existing GPU retirement fences still prevent overwriting in-flight resources. The output continues at the decoded picture resolution with the existing 10-bit intermediate format for YUV; no new HDR capability or resolution reduction is introduced.

## Shared PCM/bitstream audio

Two aligned, persistent output grains replace the repeatedly overwritten worker-stack grain. They remain alive through the output thread's drain and port close. Queueing rejects closed, stopped or failed output.

A failed drain or close retains the bounded output allocation and blocks port reuse for the remainder of the app process. A failed encoded-port close cannot be followed by reconfiguring its still-owned output as PCM. UI audio cannot take the port while that failed shutdown remains outstanding. Native playback stops with a restart instruction if PCM restoration fails, rather than looping indefinitely through recovery. A successful shutdown can still recover failed bitstream playback to PCM.

This addresses ownership and recovery, not a measured correction for an individual TV/soundbar's latency or an assertion that every receiver supports passthrough.

## Toolchain download

The OpenGL SDK URL now uses LedyBacer's maintained fork. Version 1.0.2, archive SHA-256 and rendering code are unchanged. The replacement archive was streamed and verified against the existing pinned SHA-256.

## Validation limits

See PLAYBACK_STABILITY_VALIDATION.json for final checks. Targeted app paths and the two patched libVLC units are compiled for PS5. Host lifecycle tests use real FFmpeg/software decoding with a simulated console audio boundary; the separate VLC HTTP/HLS tests use the available host libVLC, not the newly patched PS5 binary. Native console playback, real HDMI audio, final PS5 library/app linking and packaging remain pending.
