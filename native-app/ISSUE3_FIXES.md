# 0.5.34.1 issue 3 candidate

Temporary issue-test candidate based on 0.5.34. The normal release and store listing remain unchanged. Display version is 0.5.34.1; PS5 numeric contentVersion remains 05.000.035 because its package field uses three components. The updater accepts canonical three/four-component versions, so 0.5.35 sorts after this temporary revision.

Evidence: [latest report and four logs](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6264818454).

## Confirmed defects and changes

- Native/EVO: the renderer log contains FFmpeg's `Not detecting m3u8/hls with non standard extension and non standard mime type`, followed by invalid-data failure. A production-player fixture reproduces this with an extensionless HLS master served as octet-stream. Select HLS from its actual `#EXTM3U`/`#EXT-X-` header while retaining the resolved URL, authenticated custom I/O, segment restrictions and cancellation. HTML rejection remains in place.
- VLC: `:ps5vdec=0` does not reliably disable the boolean option. A real-VLC test with an instrumented rejecting hardware decoder reproduces its reselection using the old option. The retry now uses `:no-ps5vdec`, `:codec=avcodec` and `:avcodec-hw=none`, preserving source, headers and resume/pause state.
- The log shows a zero-dimension hardware decoder opening/closing without packets and subsequent CPU-workspace allocation failures. Reject hardware opens until dimensions are known, and use direct CPU workspace when the shared flexible block is unavailable. Failure-injection tests exercise ownership, reuse and cleanup; this does not simulate successful console decoding.
- The release linker set `vlc_ps5_big_alloc` and `vlc_ps5_big_free` to zero, disabling the already-compiled allocator. Remove those overrides and require both symbols to resolve to executable production functions before packaging. The check fails against the previous linked image.

## Colour candidate

Preserve planar I420/I0AL output instead of requiring VLC to convert every source to NV12/P010. Preserve existing NV12/P010 sources, pitch/crop handling and correct low/high bit alignment; interleave planar chroma in the existing owned-frame packer. Add layout diagnostics without URLs or credentials.

Real libVLC tests decode red, green and blue fixtures, verify pixel values, and exercise 8-bit and 10-bit planar pictures. These tests validate the new path; they do **not** prove that the exact PS5 red/green output has been eliminated. Affected-console confirmation remains required.

## Boundaries and retest

HTTP 403 refusals, DNS failures and provider-script HTTP 404s in the logs are separate failures. No universal provider compatibility or server-side fix is claimed. The public issue remains open. Retest the same Vidmody/MoonVidmodyFilmDizi source with Native, then a source known to play in Native with VLC. Check picture colours, audio, subtitles and stop/reopen behavior; collect playback and renderer logs before restarting.

The broad pre-fix Native network regression also encountered its existing intermittent EOF/replay timeout before reaching the new HLS case. The isolated HLS fixture is the before/after reproduction used for this report; do not present that initial broad run as a full pass.

The first post-fix HLS run exhausted its eight-second host startup budget while initializing the 256 MiB packet pool. An isolated debug allocation benchmark took 8.9 seconds under concurrent build load. The first-picture test now allows 30 seconds while still requiring decoded video and the correct timeline; production buffering and allocation behavior are unchanged.
