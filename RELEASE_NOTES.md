# Nuvio Max'd 1.0.0.10 experimental — remote remapping attempt

## Remote buttons
- Add Settings > Controls > Remote buttons (experimental) with Test remote buttons and press-to-assign mapping.
- Assign detected Media Remote or TV/HDMI-CEC buttons to Search, Options, Select/Back, navigation, play/pause, playback or seeking. Search uses the same app action as Triangle on the controller.
- Save up to 16 mappings. Change or remove individual mappings, or reset all app remote mappings. Controller controls stay unchanged as a way back.
- Ignore assignment presses, held buttons when switching modes, idle/release sentinels and inputs intercepted by system overlays. A 15-second timeout explains when no button reaches the app.

## This is only an attempt
This is not a confirmed fix for issue #16. PlayStation itself, or your TV, may handle Mute, volume, microphone, PS/Home and branded app buttons before Nuvio receives them. If the app never receives a button press, a setting inside Nuvio cannot remap it. This build does not change controls on the PlayStation home screen or in other apps.

## Testing and installation
Install PPSA99288.zip as an app folder, or use the FFPFSC image. Keep saved user data and the previous installer. Content version: 07.000.010.

Use Test remote buttons first, then assign Search to a detected button and test from Home. Please report the firmware, remote type, physical button, button name/number shown and what happened. Close/reopen to check persistence. Automated host and package checks do not establish PS5 hardware compatibility.

Existing media-server, torrent/Web Drop, HDR and playback features are retained. See TESTING.txt for the full checklist.
