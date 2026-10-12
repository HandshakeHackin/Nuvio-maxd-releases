Nuvio Max'd 0.5.21 — experimental HDR and HDMI audio test build

- Move HDR output switching into the graphics driver, with safe rollback and clearer diagnostics.
- Add optional Auto/Bitstream audio for Dolby Digital, Dolby Digital Plus and DTS core at 48 kHz.
- Keep PCM as the default; TrueHD and DTS-HD stay decoded PCM.
- Recover to PCM and re-buffer after compressed audio output fails.
- Keep native-resolution GPU YUV rendering and all existing subtitle/settings improvements.

Start with PCM audio when testing Native HDR10, then try Auto audio with a compatible receiver. Test pause, seeking, track changes and leaving playback. Bitstream bypasses the HDMI capability check for testing. Keep 0.5.20 for rollback.

Experimental build on builds/v0.5.21; main remains on 0.5.20. HDR acceptance on firmware 11.00, HDMI receiver output and motion smoothness require console testing. No Vulkan/RADV backend is included.

Download PPSA99288.zip for folder installation or NuvioMaxd-0.5.21-native.ffpfsc for image installation. Matching source, validation results and SHA256SUMS are included.
