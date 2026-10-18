# Nuvio Max'd 0.5.32 — MAX'D icon and updater shutdown fix

## Changed

- New MAX'D family app icon: angular M/play mark, prominent MAX'D lettering and smaller NUVIO label, using Nuvio teal, white and pink/purple. The PS5 app, GitHub page and tagged store icon use matching artwork.
- Guarded a missing episode-task cancellation flag during shutdown. Closing the app before starting an episode task could crash before the updater's exit handoff, causing its helper to cancel installation. This is a candidate fix for the updater portion of [issue #7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7), pending confirmation on the affected PS5.

## Important installation guidance

If Install and close crashes in your current version, install 0.5.32 manually from GitHub, or through Homebrew Store once its listing shows 0.5.32. Keep your saved app data. A newly downloaded image cannot repair the updater code still running in an older installation. The automatic image installer requires a compatible FFPFSC install, payload loader and ShadowMount+ API; folder installs use store/manual replacement.

The fresh-launch shutdown regression reproduced a segmentation fault before the fix and passes afterward. All 28 updater transaction tests pass. Full CI and native package verification evidence are included in native-validation.json. These checks do not prove PS5 helper/mount/exit behavior; affected-console confirmation is still needed.

## Existing features and open reports

The 0.5.31 provider-download and multiple-profile Continue Watching fixes are retained. Reporters confirmed those improvements; [#4](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/4) is closed. [#3](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3) remains open for the remaining difference in provider/source coverage compared with desktop. No additional provider-coverage change is included in 0.5.32.

Hebrew characters displaying as question marks, also reported in [#7](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7), remain under investigation and are not fixed in this release. Tracker setup limits and experimental HDR/EVO/VLC behavior remain as documented; Native is the default player.

See the [testing guide](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/builds/v0.5.32/native-app/TESTING-0.5.32.md). Please reply in the relevant issue if the problem persists, with the version and sanitized logs. Do not post passwords, account tokens, PINs or stream URLs.
