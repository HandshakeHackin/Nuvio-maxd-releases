# 0.5.33 test build: Hebrew and source playback

Addresses reports in public issues #3 and #7:
https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3
https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7

- Added licensed Noto Sans Hebrew regular/bold fallback fonts to all three UI font chains and the bundled libass subtitle fonts. No prior bundled font contained the Hebrew alphabet. Existing FriBidi ordering and HarfBuzz subtitle shaping are retained. Hebrew subtitle language codes he/heb/iw select the new face.
- Native HTTP playback now passes the final redirected URL to FFmpeg, so relative HLS segment URLs use the correct directory. Custom curl transport and stream request headers remain in use.
- Identifies HTML document responses with a specific non-media message instead of a generic invalid-data error. This does not bypass server authentication or browser challenges.
- When raw torrents are the only available results and peer-to-peer is off, the source list explains how many results were hidden and which setting or debrid connection is needed. It does not silently enable peer-to-peer playback. Source-count diagnostics omit stream URLs and headers.

Scope and limitations:
The supplied 0.5.32 log also contains separate HTTP 403, DNS, provider timeout and missing-script HTTP 404 failures. These are not all explained or fixed by this build. The log does not identify the response body behind its invalid-data error. Redirected HLS is a reproduced transport defect; it is not yet confirmed as that reporter's exact cause. Torrentio zero-result reporting needs console logs to distinguish configuration filtering from an addon request/ID problem.

The reporter confirmed Settings exit works in 0.5.32. Keep its shutdown guard. Automatic update installation still needs a genuine later update and compatible image/loader/ShadowMount setup.

Test Hebrew titles/descriptions/source names, mixed Hebrew/Latin text, embedded and addon subtitles; redirected HLS with relative segments; direct MP4/Matroska; and Torrentio with the intended peer-to-peer/debrid configuration. Physical PS5 UI/audio/HDR and affected-account acceptance are not inferred from host tests.
