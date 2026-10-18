# Update cancellation during shutdown — issue #7

Report: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7

The report describes a downloaded update followed by a crash after choosing
Install and close on firmware 13.60. No crash log or install format was supplied.

## Reproduced failure

`Session::shutdown()` unconditionally dereferenced `w_episode_cancel_`. This
shared pointer starts null and is allocated only when episode loading starts.
An update immediately after launch can therefore crash during shutdown.

The normal exit sequence calls shutdown before `complete_update_exit()`. The
updater helper waits for the explicit `exit` line before attempting a guarded
unmount and replacement. A crash prevents that handoff; the helper subsequently
cancels and preserves the original image. This fits the report, but does not
prove the reporter encountered this exact path. It is not firmware-specific.

## Candidate fix and checks

Guard the optional cancellation flag, matching the other episode-cancellation
sites. Keep existing cleanup, helper handoff and storage safeguards intact.

`tests/shutdown.sh` executes the exact production shutdown body with lightweight
lifecycle boundary fakes. It extracts the real class's cancellation-member
defaults instead of giving every task a flag. The fresh-launch case crashes
with SIGSEGV before the fix and passes afterward. It also checks an ordinary
exit, cancellation with an existing playback/episode task, and cleanup before
update handoff. It does not run the real PS5 loader, UI, audio or mount service.

All 28 updater transaction/feed tests pass, covering staged-file validation,
mount guards, cancellation, backups, failures and atomic replacement. The new
shutdown regression is included in the standard host checks.

## Console confirmation and workaround

Use Homebrew Store or manual installation, preserving saved app data, until the
fixed app is installed. Downloading a newer image cannot fix the updater code
that is currently running in an older version. The change is staged for 0.5.32;
no new release is published by this investigation.

On PS5, first test exiting immediately after launch without opening a video.
Then test a real future update from an installed fixed version: confirm a clean
exit, helper notification, installed version and retained saved data/backup.
Do not replace mounted images or bypass the helper's guarded unmount checks.

If the crash persists, collect `nuvio/log.txt`, `nuvio/crash-current.txt`,
`nuvio/runtime.json`, `hui/dev/app.log` and the exact helper notification. Include
the installed version, ZIP-folder versus FFPFSC install, ShadowMount+ version,
and whether a video had been opened since launch. Remove tokens and stream URLs.
