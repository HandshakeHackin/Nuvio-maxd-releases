# 0.5.32 updater and icon acceptance checks

Use manual/store installation once if your current version crashes after
Install and close. Preserve saved app data. Store availability may lag GitHub.

1. Verify 0.5.32 is installed and the new MAX'D / NUVIO icon is visible.
2. Open the app and exit immediately without opening a video. Confirm a clean
   exit instead of a crash dialog.
3. Repeat after opening/stopping a movie or episode. Saved progress should remain.
4. A full automatic-update test requires a genuine newer verified release from
   an installed fixed version. Use an FFPFSC install supported by ShadowMount+
   and the local loader/API. Do not bypass mount, signature/hash or version guards.
5. Confirm helper notification, final version, retained backup and saved data.

If it still fails, collect nuvio/log.txt, nuvio/crash-current.txt,
nuvio/runtime.json, hui/dev/app.log and the helper notification. Record installed
version, firmware, ZIP-folder vs FFPFSC install, ShadowMount+ version, and whether
playback had been opened since launch. Remove tokens, PINs and stream URLs.

The regression executes the production shutdown body with boundary fakes and
real task-flag defaults; it reproduced SIGSEGV before the fix. It does not run
PS5 audio/UI/loader/mount boundaries. All 28 updater transaction tests pass.

Hebrew question-mark rendering remains under investigation. If affected,
report whether it occurs in UI titles, subtitles or both, with subtitle format
and a sanitized example. This release does not claim to fix that part of #7.
