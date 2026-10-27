# 0.5.26 local playback timing test

Unpublished PS5 test build. Native playback remains the default. This is not a
replacement graphics backend or a new decoder release.

The UI records when it selects a new picture and observes its completed VideoOut
flip. It tracks both GPU command flips and the CPU fallback. Three valid samples
are required before an exponentially smoothed selection-to-scanout estimate
advances video selection; the adjustment is capped at 40 ms. Audio, progress,
subtitle clocks and HDMI output ownership are unchanged.

Pause, buffering, stopped playback and seek-generation changes clear timing
history. Repeated picture redraws are sampled once. Unknown markers, invalid or
old process timestamps, delays exceeding 100 ms and failed submissions are not
used. Missing/stale readback leaves video scheduling at its original clock.
The timestamp conversion follows the public VideoOut/process-time approach
reviewed in ProsperoTV's playback timing work:
https://github.com/blackbearreloaded/ProsperoTV/commit/9504e14ead5ac4d33af172318eb9ed53746e7c5f
The tracker and OpenGL submission adapters are implemented here for Nuvio.

This measures console scanout latency, not TV processing or soundbar delay.
The PCM clock still uses the 0.5.25 two-grain buffering estimate. It cannot measure
optical/HDMI receiver latency. No automatic volume boost is applied.

Retained from 0.5.25: preferred 16-bit PCM, explicit normalized stereo downmix,
Auto selecting PCM with Surround Sound off, and channel/peak/RMS diagnostics.
The controller lightbar fades teal → white → pink/purple over an 18-second loop,
with dim playback lighting and a steady teal color when Reduced Motion is on.
Console case lighting is unchanged. All saved user choices remain in place. EVO remains optional and experimental;
its previously observed allocation failure/green picture is not fixed here.

## Console test

Start with Native playback, PCM and Surround Sound off. Compare a genuine stereo
source and a 5.1/7.1 source: dialogue, music, left/right separation and lip sync.
Play continuously for at least one minute, pause/resume, and seek both directions.
Check the latest `player: scanout timing`, audio-format/level and `shown ... fps`
lines in `/download0/nuvio/log.txt` (inside the title's sandbox over FTP). Remove
stream URLs and tokens before sharing logs. Compare 60 Hz and 120 Hz only where
the TV supports them. 24 fps material can still show its inherent camera-pan
stutter; this change does not interpolate frames.

Host tests cover timestamp validation, both flip submission paths, failed-submit
pass-through, stale/duplicate samples, ring eviction, resets and the lead cap.
Host checks and package validation do not establish console playback quality.
Keep 0.5.25 available for comparison/rollback. Do not publish until console tests
confirm the new timing and retained audio behavior.
