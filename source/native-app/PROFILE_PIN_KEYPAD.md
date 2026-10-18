# Profile PIN keypad — staged, not released

Requested in issue #7:
https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/7#issuecomment-6252280864

Locked account profiles now open a controller-operated in-app numeric keypad instead of ask_text/the PS5 IME. Digits are masked; D-pad moves, confirm selects, Square deletes and back cancels. The keypad accepts up to 16 numeric characters, preserves leading zeros, and leaves PIN validity/length rules to Nuvio's existing server verification. Entering a PIN does not itself grant access to a profile.

The modal holds underlying page input, including its closing frame, and blocks automatic prompts while open. It clears its entry state and callback on cancel, submission, account/profile view reset and shutdown. No PIN is added to preferences, diagnostics, clipboard or the PS5 keyboard suggestion path. It does not erase entries the PS5 keyboard may have remembered previously.

Validation: controller-model tests cover mapping, masking, leading zeros, capacity, delete, empty submission, cancel priority, held-button suppression and navigation bounds. Wiring checks retain server verification and bypass the IME; the production shutdown-body regression checks PIN cleanup before teardown. Session and UI translation units passed syntax compilation. A profile-pin host preview mode is included for subsequent UI validation. Physical PS5 acceptance is pending.

Do not replace the distributed 0.5.33 packages or create a new build/release for this change until the reporters have tested the existing Hebrew/source candidate. Stage it for the next approved build. The current test downloads intentionally do not include this keypad.
