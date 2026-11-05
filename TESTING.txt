# Experimental Media Remote remapping attempt — issue #16

This build is an attempt, not a confirmed PS5 remote fix. Nuvio can only change buttons that PlayStation delivers to the app while it is open. PlayStation or your TV may handle Mute, volume, PS/Home, microphone and branded app buttons before the app receives anything. Nuvio cannot override those system controls or change what the remote does in other apps.

1. Install the 1.0.0.10 experimental PPSA99288 ZIP or FFPFSC, keeping saved user data. Keep the prior installer and a controller available.
2. Open **Settings > Controls > Remote buttons (experimental)**.
3. Choose **Test remote buttons**, release the remote buttons, then press one button. If it arrives, the app shows its name or a number. If nothing arrives within 15 seconds, try another button; that button may belong to PlayStation/your TV, or the firmware may not expose it to this app.
4. Choose **Add button mapping**, choose an action (for example **Search**), then press and release the remote button. Search uses the same app action as Triangle on the controller. The assignment press is consumed, so press again to test the new action.
5. Select a saved mapping to change its action or remove it. **Reset all remote mappings** restores normal app remote handling. The controller retains its normal controls as a way back.
6. Close and reopen the app to check the mapping was saved. Try navigation, playback and seeking; open/close the PS5 overlay and check held buttons do not fire unexpectedly on return.

Mappings apply to Nuvio on this console and are separate for the Media Remote and TV/HDMI-CEC input. There are at most 16 mappings. They do not remap the DualSense or PlayStation OS. Please report your PS5 firmware, whether you used the PS5 Media Remote or a TV remote, which physical buttons you tried, any button name/number shown, and what happened. Do not include account tokens or stream URLs.

Existing Plex/Jellyfin/Emby, torrent/Web Drop and HDR features remain included. Console testing is pending; host tests use simulated PS5 input samples.
