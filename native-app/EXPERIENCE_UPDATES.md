# 0.5.17 playback and accessibility controls

All preferences are local and retain existing accounts, progress and defaults.

“What’s new” appears on first launch and after a version change. Cross or Circle dismisses it and saves the acknowledged version; it can be reopened from Settings. First-time sign-in follows the notes. Startup input cannot accidentally dismiss the notes while the boot screen is still up.

- Subtitle font (Regular/Bold/Display), colour (White/Yellow/Cyan), effect (Plain/Outline/Shadow) and background opacity (0–100%). Each style and size row shows a live example in Settings, drawn by the same function as playback. Fonts retain the existing language fallback chains. Existing L2/R2 subtitle timing remains available.
- D-pad seek choices: 5, 10, 15, 30 or 60 seconds; L1/R1: 5, 10, 30, 60, 120 or 300 seconds. Defaults remain 10 and 60 seconds.
- Next-episode countdown: 5, 10, 15, 30, 60 or 120 seconds. Default remains 10 seconds. Applies during credits and at EOF; pausing, buffering or the track menu holds the countdown. Circle cancels; Cross can continue immediately. Missing timings do not cause an early transition, and source-quality matching remains required for automatic playback.
- Reduced motion makes spring scrolling, focus and menu transitions immediate and freezes decorative background motion. Video and autoplay use separate clocks.
- Higher contrast brightens text and strengthens panel/focus boundaries without changing video colours or HDR/SDR conversion.
- Background runtime snapshots capture frame timing, frame counts, source size, buffering, pause state, selected engine and requested/active output state. New snapshots whitelist fields and exclude stream URLs, account tokens and title names. Existing general log files can still include URLs; remove sensitive information before sharing those logs.

Diagnostics live beside log.txt in the title's nuvio data directory (normally /download0/nuvio, exposed through the sandbox download0 path). Settings → Playback diagnostics shows the location. Files: runtime.json, runtime-previous.json, boot-current.txt, boot-previous.txt, crash-current.txt and crash-previous.txt. Startup rotates one prior run. A clean-exit flag distinguishes orderly shutdown from an unknown/forced exit. Fatal POSIX signals write a minimal signal marker through a pre-opened descriptor; this is not a kernel crash dump and cannot capture every forced termination. Diagnostic snapshots are periodic, not a per-frame recording.

Host checks cover preference recovery, shared subtitle rendering, reduced-motion clock isolation, contrast tokens, countdown holds/cancellation/EOF, diagnostic field filtering, previous-run retention and an actual subprocess crash. PS5 build/import and archive validation follow; console behavior still requires testing. Native HDR acceptance and remaining motion judder are not marked fixed by this release.
