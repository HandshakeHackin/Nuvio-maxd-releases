Nuvio Max'd 0.5.15 native experimental — EVO Player 0.12.0 video engine.

This build adds a selectable video decoder and presentation backend from the latest EVO Player release, v0.12.0, pinned to `9ee9a5420c73b5be448610e5f0a90a29a75592f3`.

Select **Settings → Playback engine → EVO Player 0.12.0 (experimental)**, then open a stream. Nuvio native remains the default, and you can switch back from the same menu. The setting applies to the next stream.

The integration uses EVO's actual decoder dispatcher, PS5 native decoder, presentation core and clock. It preserves native-resolution owned buffers and consumes frames in timestamp order. Nuvio keeps stream selection, playback controls, audio, subtitles, Skip Intro and watch-progress sync. Both engines use the PS5 hardware decoder service.

**Console playback smoothness and native HDR acceptance remain unverified.** EVO's standalone interface, audio passthrough, interpolation and automatic refresh matching are not included. HDR output and tone mapping remain Nuvio's existing implementation. Compatible HDR10 base layers are supported; Dolby Vision Profile 5 is rejected because EVO's full reshaper requires a different FFmpeg ABI. Audio remains stereo.

Download **NuvioMaxd-0.5.15-FFPFSC.zip** for the image and installation instructions, or **PPSA99288.zip** for the app folder. Fully close the app, replace the old installation, then rescan in ShadowMount+. Keep one PPSA99288 installation and retain saved data. Check Settings → App updates shows **0.5.15**.

Compare the same SDR and HDR10 streams on both engines. Test camera pans, pause/resume, seeking, Skip Intro, audio/subtitle selection and next episode. Prefer 120 Hz can be tested where supported. Logs identify the engine and actual hardware/software backend; share the latest `player: quality` and `shown … fps` lines with stream URLs and tokens removed.

Validation: existing host regression suite and PS5 host checks pass on the exact source revision. New tests decode real H.264 SDR and HEVC 10-bit samples, drain reordered frames at EOF, flush/reopen after seeking, check buffer ownership, 24/120 presentation timing, late-frame handling and Profile 5 rejection. The native SELF links 411 supported imports. Full image extraction and ZIP hashes match all 124 packaged files. The matching source archive contains 514 verified files, including EVO source, GPL license and integration notes. Checksums and the validation report are attached.

Source revision: `0159a02e951a01e3fa14edfe3e45e6c9ca74f469`.
