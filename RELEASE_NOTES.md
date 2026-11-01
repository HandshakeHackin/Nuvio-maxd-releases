# Max’d 1.0.0.4 — subtitle and Library artwork test

Targeted prerelease for [#12 — subtitle sizing](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12) and [#13 — missing Trakt Library covers](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13). Both fixes are included in the same package. **1.0.0.3 remains the current stable release.** Please report test results in the corresponding issue; console confirmation is pending.

- Fixed plain text subtitle playback using smaller sizes than the settings preview. Small, Medium and Large now share one size definition across preview, plain text overlay and libass.
- Added Extra large (150%) and Largest (200%) subtitle sizes, relative to Medium. Existing preferences are preserved; authored ASS and bitmap subtitles retain their existing behavior.
- Fixed missing Library covers depending on metadata previously loaded by the Board. Nearby Library titles now request their own missing artwork through compatible metadata addons.
- Limited metadata work to four requests in flight, retained existing posters and kept stale profile results isolated. An addon response without a poster allows the next compatible metadata addon to be tried.
- Adjusted the larger settings preview so it fits without covering its help text.

Regression tests fail on the unpatched source and pass with these fixes. Real libass pixel tests, subtitle/bitmap/language checks, Library metadata and profile tests, settings/Continue Watching/artwork regressions and PS5 compile/link/import checks passed. Physical-console reporter acceptance remains pending.

See **TESTING.txt** in the downloads for installation steps and issue-specific checks. Close the app before updating and preserve saved user data. Package contentVersion: **06.000.004**; title ID: **PPSA99288**.
