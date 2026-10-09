# Playback quality: what reaches the screen and speakers

The goal is to play the selected source at its original resolution and preserve
its color and audio information wherever the console and connected equipment
can accept them. v0.5.6 adds experimental native HDR10 output. Source HDR
mastering metadata and surround audio are still not preserved in the output.
A 4K, HDR or Atmos badge describes the source; player controls describe the
rendered picture and selected output path.

## Current playback path

| Part | What v0.5.6 does | What it preserves or changes |
| --- | --- | --- |
| Direct/debrid link | Gives the original URL and required headers to the native player | No app-side video re-encode on this path |
| Video decode | Uses VideoDec2 where supported, otherwise FFmpeg | Decodes the selected source; software fallback does not itself lower resolution |
| Common YUV pictures | Copies visible 8/10-bit planes at the decoded dimensions | Preserves dimensions, cinema crops and sample aspect ratio |
| GPU conversion | Uploads R8/RG8 or R16/RG16 planes, produces RGB10_A2 | Preserves spatial detail and 10-bit precision; compatible HDR10 keeps PQ/BT.2020 through final composition |
| HDR10/PQ | Defaults to experimental 10-bit BT.2100 PQ VideoOut | Bypasses SDR tone mapping; source mastering-display/content-light metadata are not forwarded |
| SDR conversion | Available through Settings → HDR playback | Explicitly tone maps HDR into SDR; ordinary SDR output remains 8-bit |
| Display | Follows the PS5's configured 1080p, 1440p or 2160p resolution; manual overrides are available | Logs configured and rendered size separately; fallback to 1080p is shown. TV signal information confirms HDMI output |
| Frame loop | Paces submissions at the display backend's accepted 60/120 Hz rate | Avoids unbounded extra rendering; does not interpolate or alter the source frame rate |
| Audio | FFmpeg decodes, then resamples/downmixes to 48 kHz, 16-bit stereo PCM | Surround channels and Atmos object information are not retained in the output |
| Optional server transcoding | Requests H.264 and AAC/MP3 with at most two audio channels | A compatibility option that changes the selected source format |

Uncommon software picture formats go through an 8-bit RGBA conversion.
Pictures above 4096 pixels per side are rejected. An 8K-capable TV does not make
this build an 8K player.

The resolution selector is adapted from
[Stremio-Plus at commit `64d1bfb`](https://github.com/LoZazaMastro/Stremio-Plus/tree/64d1bfb5b78b3c6a37e013973402f3239bee5df0).
Its README and `src/video_gl.cpp` also describe an SDR output with HDR tone
mapping. Matching the PS5 output resolution is useful, but does not establish
native HDR or original audio passthrough in either app.

For a 4K TV, select 2160p in the PS5 video-output settings, then leave Nuvio on
**Follow PS5**. Nuvio's manual **4K** override is also available. A 3840x1600
movie is a cropped 4K picture: black bars preserve its aspect ratio on a
3840x2160 display. A 1080p render cannot show all the spatial detail of that
source, even though the decoder retains the original dimensions.

## Experimental native HDR10

Choose **Native HDR10 (experimental)** in Settings → HDR playback. This is the
default for compatible PQ sources tagged with BT.2020 primaries and matrix.
The YUV shader converts them to BT.2020 RGB without SDR tone mapping. The final
pass keeps the picture in PQ, composites SDR controls in linear light at
100-nit white, and packs exact 10-bit RGB10A2 words for VideoOut.

The pinned OpenGL SDK still treats its final attachment as BGRA8. The final
shader uses its four channels to transport the four bytes of each packed
10-bit pixel. Blending, dithering and sRGB conversion are disabled at this
step. VideoOut reads those same bytes as `Rgb10A2Bt2100Pq`; the picture's RGB
components are not reduced to 8 bits.

The v0.5.14 package declares the HDR-capable application profile `0x62000000`
documented by [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight/blob/a6e56cf4e8e9e469d5bc5cc06ff71ad4ca16a6a9/docs/PORTING.md).
Earlier Nuvio packages declared zero. Firmware 11.00 rejected the RGB HDR10
buffer attribute in the v0.5.13 console test even though decoding succeeded.
Build and extracted-image validation now enforce the application metadata.
This package correction still needs an HDR-output check on the console.

The adapter borrows the SDK's registered buffer set and changes its attributes
in place, following
[EVO Player at commit `9715f6e`](https://github.com/sainsaji/EVO-PLAYER-PS5/blob/9715f6ee9bf9b53461d6fdf9a2e11b4c48357783/projects/evoplayer/media/src/evo_agc_runtime.c).
GPU work and pending flips finish before switching. After presenting, the app
checks that VideoOut reports HDR dynamic range. Refusal or a five-second
confirmation timeout stops the stream with a message. Returning to the menu
restores SDR; a refused restoration retains HDR packing while it retries.
**Convert HDR to SDR** is an explicit compatibility setting, not an automatic
replacement for a failed HDR request.

This is an experimental release. Host tests verify packed pixels and simulated
transitions, but cannot confirm the PS5's HDMI signal or the TV's behavior.
Source mastering-display metadata and MaxCLL/MaxFALL are not forwarded yet;
the display uses the console/display defaults. HDR10+ dynamic metadata, HLG
and complete Dolby Vision processing are not supported. A Dolby Vision stream
needs a compatible HDR10 base picture; profile 5 is rejected in native HDR
mode. These limits prevent a claim of complete original-format preservation.

### Console verification

1. Set the PS5 to 2160p with HDR available, and leave Nuvio's display setting on
   **Follow PS5**. If changing Nuvio's resolution override, reopen the app.
2. Play a known HDR10 stream. Check that the TV reports HDR and the player
   controls show **HDR10 10-bit (output confirmed)**. Inspect bright highlights,
   dark backgrounds, black bars, subtitles and controls.
3. Pause and resume, seek, then leave playback and play an SDR H.264 or HEVC
   stream. Check the SDR picture and subsequent HDR playback.
4. Share `player: colours`, `player: presenting`, `player: quality` and timing
   lines from `nuvio/log.txt`, plus `[HDR]` lines from `hui/dev/app.log`, without
   playable URLs or account tokens. Include the TV's HDR indication.

Output-status confirmation establishes the console's reported range; it does
not prove that every HDMI/TV stage displays the intended colors or precision.

## Audio depends on the output connection too

The reported setup is a Samsung TV and Samsung soundbar connected by optical;
the model numbers are unknown. Optical cannot carry Dolby TrueHD or Dolby Atmos.
Some compressed surround formats may work, but multichannel PCM requires a
suitable HDMI connection. Samsung documents these differences in its
[soundbar connection guide](https://www.samsung.com/ie/support/tv-audio-video/benefits-of-soundbar-tv-connections/).

The app can pursue multichannel PCM without mixing every source to stereo.
That still requires explicit channel mapping, precision/rate handling, output
configuration, and clock/track-switch tests. It would not preserve Atmos objects
or DTS:X metadata. It must not be described as bitstream passthrough.

[EVO Player's passthrough research](https://github.com/sainsaji/EVO-PLAYER-PS5/blob/main/docs/research/audio-passthrough.md)
reports failed passthrough tests on its tested PS5 firmware and uses multichannel
PCM instead. These findings are a platform constraint to investigate, not proof
of identical behavior on firmware 11.20. Compressed audio must never be sent to
a normal PCM port as though it were decoded samples.

## Choppy playback and moving noise remain under investigation

The latest console report describes choppy 1080p HEVC, apparently smooth 4K,
and haze or moving background pixels in the 4K picture. The pixels stop moving
when playback is paused. That observation does not establish whether they are
source grain, a color-conversion issue or a decoder/presentation fault.

No denoising, sharpening, interpolation or resolution reduction has been added
to hide the symptom. Diagnosis needs the affected stream's `player: presenting`,
`player: colours`, `hwdec:` and `player:` timing lines, particularly shown frame
rate, drops and buffer duration. Leave out playable URLs and account tokens.
The separate `hui/dev/app.log` can establish display/renderer behavior.

GPU upload/composition costs and the audio clock still need measurement on the
console. A 24 fps source on a 60 Hz display
also has uneven frame repetition; this is different from decoding below the
source frame rate. The display's actual accepted refresh rate matters.

The 22 host video/pacing tests include an
actual H.264 High clip and one-pixel 3840x2160 detail through the production UI
renderer, 877 distinct grayscale levels after 10-bit GPU conversion, and
60/120 Hz scheduling over one minute with hitch recovery. HDR checks cover all
1,024 RGB output levels, channel ordering, one-pixel cropped 4K detail, 100-nit
UI white and linear-light shadows. Seven transition tests simulate refused
requests, post-flip confirmation, timeout, SDR restoration and handle ownership.
The output-selector tests verify 27 assertions using fake VideoOut calls.
These checks establish that the tested host path retains native spatial detail
and HDR precision. They do not reproduce the PS5 GPU,
prove the actual HDMI signal, or establish that the reported haze and stutter
are fixed. Player controls now show decoded picture dimensions and bit depth,
render resolution, accepted refresh, color conversion and stereo audio.
