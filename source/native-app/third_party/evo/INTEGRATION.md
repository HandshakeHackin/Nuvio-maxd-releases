# EVO Player 0.13.0 integration

Source: sainsaji/EVO-PLAYER-PS5, release v0.13.0, commit
`2db44b5294657b12bf91666154a05f029ddb5aa2` (latest release when integrated).
The files in UPSTREAM.json are unmodified copies, checked before every build.
The upstream GPL-3.0 license is included here and in every app package.

The selectable EVO backend compiles EVO's real video decoder dispatcher,
native sceVideodec2 backend, pp_playback presentation core and pp_clock.
`src/evo_backend.cpp` connects those modules to Nuvio's existing network input,
demux/audio/subtitle threads, owned native-resolution frame queue and GL video
surface. EVO's PTS-order presentation and late-drop policy replaces Nuvio's
newest-due-frame selection when this backend is selected. Unknown timestamps
advance at the stream frame rate. Packet send backpressure is drained/retried.
Seeking flushes the EVO decoder; queue serials invalidate old pictures, and
reset the EVO presentation clock. Pause and buffering hold the current owned
picture and stop queue consumption. The frame queue retains copies so decoder
pool reuse cannot race GL uploads. No CPU downscaling is added.

This is a video-engine integration, not the entire standalone EVO application.
Nuvio retains its controls, progress, audio output, HDR compositor, colour
conversion, VideoOut ownership and display-refresh setting. EVO's RmlUi shell,
standalone file browser, audio passthrough, interpolation, automatic refresh
switching and network player are not included. HDR acceptance still needs a
console test. Use Prefer 120 Hz for the existing compatible-film cadence test.

`src/evo_runtime.cpp` sends EVO diagnostic messages to the existing Nuvio log
and implements its host logging/breadcrumb hooks. EVO's Dolby Vision parser
requires FFmpeg 7.1.1 private structures, so it is not linked against this app's
different FFmpeg ABI. The compatibility hooks return no reshaping metadata,
and Profile 5 is explicitly rejected before decoder creation. Compatible
HDR10 base layers retain Nuvio's colour policy. Full Dolby Vision is not claimed.

Hardware decode watchdog notifications are retained from EVO. The Nuvio thread
lifecycle still joins decoder workers; its recovery from a non-returning PS5
decoder syscall is not validated. Such a hang may require closing the title.
Do not describe host validation as proof of console decoding, HDR, cadence or
watchdog recovery.

Nuvio native remains the default. Select Settings → Playback engine → EVO
Player 0.13.0 (experimental); the choice applies to the next stream. Compare
the same SDR and HDR10 streams, pause/resume, seeks/Skip Intro, audio/subtitle
selection and next episode. The player log identifies the engine and actual
hardware/software backend and retains shown-fps/drop/buffer statistics.

0.13.0 refresh: enable the upstream VP9 Profile 2 slot and 4K level 5.1.
The standalone audio-lead and scrub-preview defects do not apply: Max’d uses
its own PCM clock and commits seeks without waiting for a thumbnail worker.
