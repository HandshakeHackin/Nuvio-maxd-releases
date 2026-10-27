# Install source and update reminders

Version 0.5.20 asks once where Nuvio Max'd was installed: Homebrew Store,
GitHub/manual download, or Not sure. Existing settings have no source and receive
the same one-time prompt. Circle saves Not sure; the selection is editable in
Settings → Install source. The choice is saved locally in settings.json and is
not uploaded or used as an install/download counter.

After onboarding, one background check per launch reads the existing public
release feed. Offline/background errors stay quiet and can be retried through
Settings → App updates. A newer release triggers one reminder for that version
per launch, deferred while playing or while a dialog/sign-in is open.

Homebrew Store selections receive store instructions and no in-app installation
offer. The GitHub feed does not prove the store has published that version, so
instructions explicitly say to check the store; its catalog can lag. GitHub/manual
selections retain verified in-app image installation and can also view manual
GitHub instructions. Not sure directs users to choose a source.

This is a user-selected identifier, not automatic proof of installer origin.
Regression checks cover migration, invalid values, persistence, routing and
rendered selector/update dialogs. Playback and saved account data are unchanged.

Settings also includes an optional Support on Ko-fi QR page pointing to
https://ko-fi.com/handshakehackin. It never starts account pairing or
authentication polling, and closing it does not change sign-in preferences.
