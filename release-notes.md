Nuvio Max'd 0.5.14 native experimental.

This release corrects the missing HDR-capable application declaration associated with the `0x80290003` rejection reported on firmware 11.00. The package now declares the public VideoOut profile used by ProsperoLight and EVO Player. Native 4K/PQ/10-bit packing is preserved, and build/package validation now enforces the declaration.

**HDR acceptance and remaining motion judder still need console confirmation.** Hardware decoding succeeded in the reported failure; this correction targets output setup. It does not establish that every frame drop or motion hitch is fixed.

Changes since the public 0.5.9 release:

- Connected Services: pair TorBox by phone QR or API key, connect Premiumize by API key, or explicitly import supported connections from Nuvio Sync profile 1. Resolve cached original files when addons return clientResolve instructions.
- Instant playback: prepare original-file links while browsing, then reuse them at launch. Configurable candidate count, limited background requests and a short-lived in-memory cache. Video still buffers before playing.
- HDR-to-SDR conversion: revised luminance mapping, source peak metadata and neutral-axis gamut fitting for the explicit SDR option. Native HDR bypasses this conversion.
- Display refresh: optional Prefer 120 Hz, with accepted-rate readback and no resolution reduction. Restart the app after changing this setting.
- Episode prompts: cross/circle controller symbols after D-pad Down selects Skip Intro, Skip Recap, Skip Credits or Next Episode.
- Logs identify the build; playback statistics distinguish paused or buffering playback.

Download **NuvioMaxd-0.5.14-FFPFSC.zip** for an image with instructions, or **PPSA99288.zip** for the app folder. Fully close the app, replace the previous image/folder, then rescan in ShadowMount+. Keep only one PPSA99288 installation; retain saved data. Check Settings → App updates shows **0.5.14**.

To test the HDR correction, select **Native HDR10 (experimental)** and retry the same source. Check that the TV enters HDR and playback details reach **HDR10 10-bit (output confirmed)**. The startup menu's SDR 8-bit label is normal. If it fails, share the version, `player: output failed` and `[HDR]` lines with playable URLs and tokens removed.

The public app update feed can discover this release. The ProsperoStore catalog still pins 0.5.9 until a separate catalog update is accepted.

Validation: 227 host tests and 27 display-selection assertions passed on the exact source revision; PS5 host checks passed. Native SELF links 409 supported imports. Full FFPFSC extraction and store ZIP verification match all 121 files and confirm the corrected HDR declaration. The matching source archive verifies 491 files. Checksums and the validation report are attached.

Source revision: `0e0c0cf03a88153000c0a5e80f61437b83246b31`. The matching archive contains the released native source and dependency/license notices; the development repository remains private.

HDR10+ dynamic metadata, complete Dolby Vision processing and forwarding source mastering metadata remain unsupported. Audio output is stereo. Live provider authentication, HDMI/HDR acceptance and automatic image installation still need console testing.
