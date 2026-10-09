# Nuvio Max'd

Nuvio Max'd brings Nuvio to a jailbroken PlayStation 5 as a native, fullscreen app.
The interface, addons and player run together on the console. Downloads include
a **ZIP folder** for ProsperoStore and ShadowMountPlus, and an **FFPFSC** image
for people who prefer image installs. You don't need a PC running a companion
Nuvio server. This is an unofficial client, previously named Nuvio PS5.

Sign in with a QR code to load your addons and saved library from your primary
Nuvio profile. **Watch Progress now uses Nuvio Sync**, so you can pause on the
PS5 and pick up where you left off on another Nuvio device.

**Current release: [v0.5.14 native experimental](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v0.5.14-native-experimental).**
Download `PPSA99288.zip` or `NuvioMaxd-0.5.14-native.ffpfsc`. The validation report,
matching source archive and `SHA256SUMS` are included. The development repository
stays private; the source for each released version is available with its public
downloads.

Nuvio Max'd is also available in the
[ProsperoStore homebrew shop](https://homebrew.page/app/PPSA99288/).
Find **Nuvio Max'd** in the store to install it, or use the GitHub downloads.

## What you can do

- Sign in with your phone and keep your login between launches.
- Bring in enabled standard addons from your primary profile, including
  AIOMetadata and AIOStreams, or add a manifest yourself in Settings.
- Browse addon catalogs, search for titles and open your saved library.
- Play direct and debrid links using the built-in player. Debrid comes from
  your addon configuration; peer-to-peer torrents are off by default.
- Compare streams using addon filters and cards showing file size, language,
  quality, codec and audio badges when the addon provides those details.
- Sync movie and episode resume positions with your other Nuvio devices.
- Match the PS5 output resolution automatically, with manual display choices.
- Browse artwork with smooth poster carousels, choose seasons from a visible row,
  and pick episodes from larger thumbnails with synced progress bars.

## Install on your PS5

You'll need a working jailbreak, a compatible **kstuff-lite 1.07+** setup and
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus). Follow their
instructions for your firmware, including the requirements for experimental
PFS/FFPFSC mounting.

### ZIP folder

Extract `PPSA99288.zip` and copy its `PPSA99288/` folder into a `/homebrew/`
folder scanned by ShadowMount+, such as `/data/homebrew/`. The resulting path
should be `/data/homebrew/PPSA99288/eboot.bin`, with no extra folder level.
Set the folder and its contents to `0777` if your FTP client changes permissions.
Let ShadowMount+ finish scanning, then open **Nuvio Max'd**.

### FFPFSC image

1. Download `NuvioMaxd-0.5.14-native.ffpfsc` and put it in a `/homebrew/` folder
   on storage that ShadowMount+ scans, just like your other homebrew apps.
   For a USB drive, that might be:

   ```text
   /mnt/usb0/homebrew/NuvioMaxd-0.5.14-native.ffpfsc
   ```

   Leave the image in that folder so ShadowMount+ can identify it automatically.
2. Run ShadowMount+ and let its scan/install cycle finish. Open **Nuvio Max'd**
   from the home screen.
3. Scan the QR code with your phone, sign in to Nuvio and approve the device.
   Press Circle to continue as a guest instead.

After a console reboot, load the required jailbreak and mounting payloads again
before opening Nuvio Max'd.

The project targets firmware **9.00 and above** where the required jailbreak,
kstuff-lite and ShadowMount+ work. **Firmware below 9.00 needs testing.** Console
feedback so far comes from **11.20**; other versions in the target range remain
unverified. The v0.5.14 HDR correction still needs console confirmation.

### Updating an existing installation

Close the app first. For image installs, delete the old `.ffpfsc` and replace it
with the new image in `/homebrew/`. For folder installs, replace the old
`PPSA99288/` app folder with the one from the ZIP. Use one install format at a time.
ShadowMount+ matches the existing app by its `PPSA99288` title ID, so it
recognizes the replacement as an update.

Keep your app data to retain your login, addon configuration and local progress.
The title ID and `/download0/nuvio` data folder are unchanged. Starting with
0.5.7, Settings → App updates also offers a verified update through the bundled
helper when the public feed and required local services are available.

## Set up Watch Progress across devices

1. Sign in to the **same Nuvio account** on the PS5 and your other device.
2. Use the **primary profile** on both devices. On your other device, choose
   **Nuvio Sync** as the Watch Progress source.
3. Play a movie or episode, then pause or stop it. Open that title on the other
   device and check its resume position.

The PS5 saves and syncs progress every **30 seconds** during playback, and when
you pause or stop. It checks for remote progress at startup/sign-in and every
minute while you're browsing. For an immediate refresh, open **Settings → Watch
Progress** and press Cross.

If you're offline, updates are saved on the PS5 and retry when the service is
available. Guest progress stays on the console. Secondary profiles and
Trakt/Simkl progress sources aren't supported yet. Playable stream URLs and
playback headers aren't included in progress uploads.

## Use your debrid addons

Configure your debrid service in your addon, such as AIOStreams, using your
usual Nuvio setup. After changing that configuration, choose **Settings → Reload
addons and catalogs** on the PS5.

Addons can return playable direct/debrid links or supported cached-file resolution
instructions for a connected provider. Leave **Peer-to-peer torrents** off if you
want only direct/debrid results.

### Connected Services in v0.5.14

Open **Settings → Connected Services**. Connect TorBox with a
phone QR code or API key, or Premiumize with an API key. If you've already
connected them in Nuvio, choose **Import connections from Nuvio Sync** while
signed in to copy the connections from your primary profile.

Enable **Resolve playable links** and choose **Resolve with** to prefer TorBox
or Premiumize. AIOStreams results that ask the client to resolve a cached file
can then play through the connected service. The app requests the original
file and checks that it is cached before proceeding. Provider-specific results
stay with their named provider, and an unavailable file shows a retry/setup
message. Existing direct links continue to work as before.

**Instant playback** prepares cached original-file links as you browse, starting
with the focused card and respecting the addon filter. Ready cards reuse their
prepared links at launch. You can prepare one to five candidates or switch
this off. Continue Watching and matching next episodes use the same resolver.
The app still buffers video before playing.

This test build includes connections, cached-file resolution, provider choice,
background link preparation and read-only Sync import. Cloud Library remains
planned. See the [Connected Services guide](native-app/CONNECTED_SERVICES.md)
for setup and console checks. v0.5.14 is a public experimental release; live
provider authentication and HDR output still need console testing.

## Browse films and series

The native C++ interface is built with BlackBearReloaded's
[ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui).
Poster rows scroll smoothly, artwork
crossfades as you browse, and playback controls sit in a translucent panel.

Films have a large poster beside their details and streams. Series have a wide
episode carousel with thumbnails, descriptions and watch-progress bars. Use
Left/Right to browse episodes, or press Up to reach the season row. Choose a
season with Left/Right and Cross, then press Down to return to its episodes.
L1/R1 also switches seasons. Missing episode thumbnails fall back to the series
artwork when available.

## Choosing a stream

| Button | Action |
| --- | --- |
| Cross | Play the selected stream |
| Circle | Go back |
| L1 / R1 | Switch addon filters |
| Up from the first stream | Move to the All/addon filter row |
| Triangle or the reload icon | Refresh available streams |

Cards use the addon's stream name, description and filename to identify the
release. Size and language appear where available, with built-in badges for
quality, HDR/Dolby Vision, codec and audio. Custom image-badge rules from other
Nuvio apps aren't synced yet. A badge describes the source file; it doesn't
promise that the PS5 outputs that format.

## What's new

### Public 0.5.14 experimental release

- The package now declares the HDR-capable application profile used by
  ProsperoLight and EVO Player. 0.5.13 still returned `0x80290003` on firmware
  11.00 with the revised RGB HDR10 format; its package had omitted this required
  declaration. Build preflight and extracted-image checks now enforce it.
  This correction needs confirmation on the PS5; native 4K/PQ/10-bit packing
  stays the same.
- Skip Intro, Skip Recap, Skip Credits and Next Episode show cross and circle
  controller symbols after pressing D-pad Down. Short action labels replace the
  longer written button instructions.
- The log identifies the running build at startup, and playback statistics mark
  paused or buffering playback so a stopped frame counter is easier to understand.

This includes the playback changes from 0.5.12:

- A revised native HDR10 output format for the rejection reported on firmware
  11.00, with clearer failure messages. This still needs console confirmation.
- A new HDR-to-SDR conversion for shadow detail and colour balance, using the
  stream's peak metadata where available. Native HDR keeps its original PQ values.
- Settings → Display refresh offers **Prefer 120 Hz** for compatible displays.
  Restart the app after changing it. Resolution stays as selected and the player
  shows the accepted refresh rate; unsupported outputs retain 60 Hz.

The public download and updater offer 0.5.14 as an experimental release.

### Public 0.5.9

- **Nuvio Max'd:** A distinct name, icon and startup wordmark. The title ID and
  saved data remain compatible with earlier Nuvio PS5 builds.
- **Public downloads:** ZIP and FFPFSC packages, exact source archives and
  checksums are published together. The updater uses the public Max'd feed.

### Episode features from 0.5.8

- **Skip Intro and Skip Recap:** Buttons appear when IntroDB has timings for the
  episode. Skip Credits stops before a reported post-credit scene.
- **Next episode during credits:** The app looks up fresh streams in the
  background and shows a 10-second countdown when a matching source is ready.
  It prefers the same release group, then the same addon, while retaining the
  advertised resolution, source format, HDR, codec and audio traits. If no match
  is available, you choose a stream. Pausing, buffering or opening the track menu
  pauses the countdown.
- **Controller controls:** D-pad down selects the skip/next cards, left/right
  changes cards, and Cross confirms. Circle cancels automatic next-episode
  playback for the current episode, including at the end. D-pad up returns to
  the regular controls. The touchpad still pauses and resumes.

Timings depend on IntroDB coverage and may differ between releases. Titles need
an IMDb identity from their metadata; missing or invalid timings leave playback
alone. Settings lets you turn off skip buttons or autoplay. No credit timings
means next-episode autoplay waits until the file ends. Known post-credit scenes
are kept; an unlabelled tail after credits is left to play through.

### Updater from 0.5.7

- **App updates:** Settings → App updates checks a public release feed and verifies
  the FFPFSC before offering **Install and close Nuvio Max'd**. A bundled helper retains
  a backup, replaces the released image and requests a ShadowMount+ rescan.
- **Matching source archives:** Each release includes the native source and build
  instructions for that exact version. The development repository remains private.

Automatic installation is experimental and needs ShadowMount+ 1.7, the local
payload loader and a fully released backing image. Folder installs use manual
replacement or ProsperoStore. See the [update guide](native-app/UPDATING.md)
for the requirements, backups and remaining console checks.

### HDR features from 0.5.6

- **Experimental HDR10 playback:** Compatible PQ/BT.2020 pictures keep their
  10-bit values and use HDR10 VideoOut instead of the SDR tone map. Controls and
  subtitles are composed at 100-nit white. The app checks output status after
  the first flips and restores SDR when you leave playback.
- **An explicit SDR option:** Settings → HDR playback defaults to **Native HDR10
  (experimental)**. If the PS5 refuses HDR, playback stops with a message.
  Choose **Convert HDR to SDR** yourself if your display needs it.

### Features from 0.5.5

- **More room for artwork:** Film posters and series episode thumbnails get
  larger layouts. Series have a selectable season row and a smooth horizontal
  episode carousel. The player has a translucent transport panel and track menu.

- **Follow PS5 output:** The app reads the console's configured resolution before
  opening its display, following the approach in
  [Stremio-Plus](https://github.com/LoZazaMastro/Stremio-Plus). Settings also offers
  1080p, 1440p and 4K overrides; close and reopen Nuvio Max'd after changing one.
- **More precise video conversion:** Common YUV pictures use a 10-bit RGB
  intermediate at the original decoded dimensions. SDR output remains 8-bit;
  the new experimental HDR10 path preserves 10-bit values through final output.
- **Frame timing and playback details:** Rendering is paced at the refresh rate
  accepted by the display backend. Player controls show decoded picture size,
  bit depth, render resolution, HDR/SDR output status and stereo audio. Pauses
  and buffering no longer distort the reported playback frame rate.
- **Faster DNS failure recovery:** Parallel downloads stop after a small shared
  DNS failure budget, including when failures arrive one at a time.

### Features carried forward

- **Nuvio Sync Watch Progress:** Cross-device movie and episode resume, with
  saved offline updates and a manual refresh in Settings.
- **Native-resolution video:** Common decoded formats retain their original
  dimensions through GPU color conversion. This removes the old 1080p
  intermediate and corrects the 10-bit interpretation behind some green pictures.
- **H.264 setup recovery:** Unused codec workspaces are released when switching
  codecs, and failed H.264 setup gets one retry with a smaller pipeline.
- **Network recovery:** Failed downloads share a cooldown and stop after a
  bounded retry budget, avoiding hundreds of new requests during a DNS outage.

## Current limitations and troubleshooting

The blank home-page issue reported with AIOMetadata is still being investigated.
The supplied log showed an empty Calendar catalog as the only requested home row.

Native HDR10 output is experimental and still needs a PS5/TV check. Source
mastering-display and content-light metadata are not forwarded yet. HDR10+
dynamic metadata, HLG and full Dolby Vision processing aren't supported; Dolby
Vision playback needs a compatible HDR10 base picture. Audio output is stereo,
including sources labeled Atmos. DRM, arbitrary JavaScript plugins and external-player-only
links aren't supported. Videos above 4096 pixels per side are rejected.

Recent console feedback reports choppy 1080p HEVC and haze or moving background
pixels in otherwise smooth 4K playback. Those symptoms remain under investigation.
See [playback quality and output limits](native-app/PLAYBACK_QUALITY.md) for what
the current player preserves and what HDR metadata and surround output still need.

The release passes **168 host tests and 27 display-selection assertions**. Its
image is fully extracted and every packaged file compared with the build output. These checks cover the app
code and file formats; playback and live Nuvio Sync still need testing on a PS5.

For a playback, addon or sync problem, check `/download0/nuvio/log.txt`. Through
FTP, the path is usually:

```text
/mnt/sandbox/PPSA99288_000/download0/nuvio/log.txt
```

Share the relevant `addons:`, `catalog`, `player:`, `hwdec:`, `net:` or `progress:`
lines, along with your app version and firmware. For picture issues, include
`display:`, `player: presenting`, `player: quality` and the `shown … fps` lines.
For HDR tests, also share the `[HDR]` lines from `hui/dev/app.log` and whether
the TV reports HDR when playback starts. Leave out playable URLs and account tokens.

## Source and credits

See the [native build and test guide](native-app/README.md),
[third-party notices](native-app/THIRD_PARTY.md) and
[GitHub release guide](ps5/GITHUB.md).

This is an unofficial client. Nuvio branding and account protocols come from
[NuvioTVSmart](https://github.com/NuvioMedia/NuvioTVSmart). The native UI and
player are adapted from
[unofficial-stremio-ps5-port](https://github.com/Sp9nky/unofficial-stremio-ps5-port),
with the components and licenses listed in the third-party notices.

The browser/bridge code under `js/` and `ps5/` is retained from the earlier 0.3
experiment. Its two-package installation guide is archived; use the ZIP or
FFPFSC instructions above for the current app.
