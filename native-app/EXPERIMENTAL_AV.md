# Experimental HDR and surround audio

This branch tests driver-owned HDR output changes and optional compressed HDMI
audio. It retains the current OpenGL renderer and native/EVO decoder choices.
Host tests do not establish that HDR or bitstream output works on PS5 firmware
11.00; the installed console must accept the output format.

## Console checks

1. Start with PCM audio and Convert HDR to SDR. Confirm normal playback, pause,
   forward/back seeking, subtitles, and return to the library.
2. Select Native HDR10 and play an HDR10 stream. Check that the TV enters HDR
   and that the playback quality line confirms output rather than "checking".
   Save the `[HDR]` lines, including the driver change/register/restore results,
   if output fails. Test return to SDR with an ordinary SDR stream.
3. With an HDMI receiver/soundbar, select Auto audio output. Play 48 kHz AC-3,
   E-AC-3, and DTS core tracks and check the receiver's format display. Repeat
   pause, forward/back seek, audio track switching, and leaving playback.
4. Auto requires the HDMI monitor to report support. Bitstream bypasses that
   capability check for testing; it still falls back when opening or feeding
   compressed output fails. PCM is the default and the recovery path. TrueHD,
   DTS-HD, and unsupported sample rates remain decoded PCM.
5. Compare 24 fps playback at 60 and 120 Hz using the `shown ... fps`, dropped,
   and buffered log lines. A 120 Hz display can avoid the unequal 3:2 cadence
   of 24 fps at 60 Hz; it does not add motion interpolation.

Leave URLs and account tokens out of shared logs. Relevant files are
`download0/nuvio/log.txt` and `download0/hui/dev/app.log` in the app sandbox.

## RADV investigation

Reviewed the public Mortalfin player, hardware decoder, HDR compositor and the
PS5 Vulkan Template / PS5 Vulkan stack on 2026-10-12:

- https://github.com/mortaljinx/mortalfin
- https://github.com/mihawk-99/PS5_VulkanTemplate
- https://github.com/mihawk-99/PS5_Vulkan

Reviewed revisions: Mortalfin `f46225a149d13118126cffeb32c68e8a10e91ea6`,
Vulkan Template `91179df760df1e6c6f2d4d172bc435319d79ff3c`, and PS5 Vulkan
`252bf912a745e3566a93b69b098bdc8ec4048837`.

Mortalfin uses Sony Videodec2 for hardware decoding, not Vulkan Video decoding.
Its player converts frames with libswscale into RGBA, limits that conversion
to 1920x1080, and uses RGBA64 for native PQ HDR. Its Vulkan HDR compositor
uploads those pixels through mapped staging memory and copies to an image.
The current Nuvio renderer uploads native-resolution YUV planes and performs
colour conversion and HDR-to-SDR mapping on the GPU. Copying Mortalfin's full
path would therefore introduce CPU conversion and could reduce picture detail.

A RADV experiment should preserve native-resolution YUV and the current colour
math, and isolate the renderer from decoding. It needs:

- a pinned PS5 RADV/Mesa/platform build and a Vulkan UI composition path;
- R8/RG8 and R16/RG16 YUV uploads, explicit buffer/image lifetime and fences;
- the existing GPU tone mapping, PQ retention, and subtitle composition;
- swapchain HDR negotiation, HDMI confirmation, and SDR restoration;
- comparison against OpenGL on identical streams, firmware and refresh settings.

Measure upload/conversion time, presentation intervals, dropped frames, CPU
work, and visual quality at native 4K. Keep OpenGL selectable during testing.
Do not describe Vulkan as faster or as an HDR fix until console measurements
support those claims. This build does not include a RADV backend.
