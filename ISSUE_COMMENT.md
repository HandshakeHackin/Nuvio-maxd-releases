@redfields187 I’ve made an **experimental 1.0.0.10 test build** for this request. It lets you remap buttons the app detects to Search or other actions, including navigation, Options, playback and seeking.

[Download the experimental build](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.10-native-experimental) — use **PPSA99288.zip** for the app-folder install, or the FFPFSC image if that is how you normally install it. Keep your saved data and the previous installer.

To try it:
1. Open **Settings → Controls → Remote buttons (experimental)**.
2. Choose **Test remote buttons**, release the remote buttons, then press one. The app will show a name or number if it receives the press. It waits up to 15 seconds.
3. Choose **Add button mapping**, select **Search** (or another action), then press and release the button you want to use. Press it again from Home to test Search; it uses the same action as Triangle on the controller.
4. Select a saved mapping to change or remove it. **Reset all remote mappings** restores normal app remote controls. Keep a controller handy: its controls stay unchanged.
5. Close and reopen the app to check the mapping stays saved.

**This is only an attempt, not a confirmed fix.** PlayStation itself, or your TV, may handle Mute, volume and branded buttons such as Disney+ or Spotify before Nuvio receives them. If Nuvio never receives a button press, a setting inside the app cannot change what that button does. Even if a press reaches Nuvio, PlayStation or the TV may still perform its normal action. This build does not remap the PlayStation home screen or other apps, and it may not be possible to remap those particular buttons.

The build has passed automated checks, but I need your help confirming how your real remote behaves. Please let me know your PS5 firmware, whether you used the PS5 Media Remote or a TV remote, which buttons you tried, any name/number the test screen showed, and what happened. If a branded button simply opens its usual app, please mention that too. Please leave account tokens and stream URLs out of any logs.
