# Native 4K GPU video paths

Source review, 12 October 2026. A 4K display target alone does not prove the
decoded picture remains 4K. These are implementation findings, not a benchmark
of Nuvio against another app on firmware 11.00.

| Application | Video path found | Useful reference |
| --- | --- | --- |
| [PS5 VLC](https://github.com/ZiZc3/VLC) | Hardware H.264/HEVC up to 3840x2176; uploads decoded NV12/P010 or planar YUV planes to Vulkan images. Fragment shader converts YUV and handles HDR. | Native-resolution YUV on RADV, staging uploads, HDR swapchain changes. |
| [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight) | Videodec2 surfaces are sampled directly by AGC; true 3840x2160 source can render 1:1 at 4K. Published validation distinguishes this from upscaling. | Decoder surface ownership, GPU completion fences, direct YUV sampling, output restoration. |
| [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5) / [Jelly5](https://github.com/02dnot/Jelly5) | AGC YUV shader pipeline supports native NV12 and 10-bit planes. Aligned direct decoder buffers bypass staging; other frames use a bounded staging ring. | Direct-buffer path, bounded uploads and frame lifetime; native 4K geometry. |
| [ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV) | AGC presenter handles NV12/Main10 and native 2160p. Its overlay path can require a YUV copy. | Direct YUV presentation alongside an OpenGL interface. |
| [Mortalfin](https://github.com/mortaljinx/mortalfin) | Videodec2 decoding, then CPU libswscale conversion capped at 1920x1080, followed by Vulkan RGBA upload/composition. | HDR composition and audio ideas; not the preferred native-4K video conversion reference. |

Nuvio already preserves native 4K and converts YUV on the GPU. Its current
frame queue copies decoded planes into owned CPU buffers before GL texture
upload, which protects them from decoder reuse. Removing that copy requires
explicit decoder-buffer leases and GPU fences: a pointer alone is unsafe when
decoding can overwrite the picture while the renderer still reads it.

The strongest next experiments are a direct-buffer path for our existing
renderer, and a separate Vulkan YUV backend based on the VLC approach. Compare
the same files, output resolution and refresh mode; measure copying/upload,
conversion, presentation intervals and dropped frames. Keep our colour math,
native picture detail, subtitles and renderer fallback in both experiments.
No performance advantage or firmware 11.00 HDR compatibility is established.

## Source evidence

- VLC: [`src/player.cc`](https://github.com/ZiZc3/VLC/blob/d46be6a68f4244466b041c2dc1b640915ac1a983/src/player.cc), [`shaders/video.frag`](https://github.com/ZiZc3/VLC/blob/d46be6a68f4244466b041c2dc1b640915ac1a983/shaders/video.frag), [`ps5vdec.c`](https://github.com/ZiZc3/VLC/blob/d46be6a68f4244466b041c2dc1b640915ac1a983/ps5/modules/ps5vdec.c).
- ProsperoLight: [`native_agc_present.cpp`](https://github.com/blackbearreloaded/ProsperoLight/blob/78947e272566bbac31a5baab8965bf73cea87740/src/native_agc_present.cpp), [`VALIDATION.md`](https://github.com/blackbearreloaded/ProsperoLight/blob/78947e272566bbac31a5baab8965bf73cea87740/docs/VALIDATION.md). The true-4K-source oracle reports 600 frames at 119.87 fps, with an independent 12.70 test; this is not evidence for 11.00.
- EVO: [`evo_agc_runtime.c`](https://github.com/sainsaji/EVO-PLAYER-PS5/blob/9ee9a5420c73b5be448610e5f0a90a29a75592f3/projects/evoplayer/media/src/evo_agc_runtime.c), especially `stage_plane` and `evo_agc_blit_yuv`.
- Jelly5: [`evo_agc_runtime.c`](https://github.com/02dnot/Jelly5/blob/0916a3052f8f41f6af37db599f7db0853abb7a38/app/engine/media/src/evo_agc_runtime.c).
- ProsperoTV: [`iptv_native_agc_present.c`](https://github.com/blackbearreloaded/ProsperoTV/blob/f3142ce3ccb4a1d125b08a58fde1b5723325e4ea/src/iptv_native_agc_present.c).
- Mortalfin: [`jf_player.cpp`](https://github.com/mortaljinx/mortalfin/blob/f46225a149d13118126cffeb32c68e8a10e91ea6/ps5/src/jf/jf_player.cpp), [`jf_hdrpass.cpp`](https://github.com/mortaljinx/mortalfin/blob/f46225a149d13118126cffeb32c68e8a10e91ea6/ps5/src/jf/jf_hdrpass.cpp).
