<p align="center">
  <img src="sce_sys/icon0.png" alt="Nuvio Max'd app icon" width="176" height="176">
</p>

<h1 align="center">Nuvio Max'd</h1>

<p align="center"><strong>Your Nuvio library, live TV and watch progress — on PS5.</strong></p>

<p align="center">
  <a href="https://prosperostore.net/app/PPSA99288/">Homebrew Store</a> ·
  <a href="https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/tag/v1.0.0.26-native">Downloads and release notes</a> ·
  <a href="https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues">Report a problem</a>
</p>

**Official release: 1.0.0.26**

**Development branch: 1.0.1.0** — adds multilingual search, title suggestions and optional top navigation, and combines IPTV sources and enabled free channels in one TV guide, with optional free live TV, a choice of start page, remembered TV groups, channel rename/hide controls and backup channel selection, alongside offline release-file checks. Published downloads below remain 1.0.0.26.

Nuvio Max'd runs directly on a jailbroken PS5. Sign in with your phone or use
Guest mode, then browse your library, addons, TV playlists and media servers.
No companion PC app is required.

## What's new

- **Better seeking**, including paused seeks and changing the seek target.
- **Subtitle fixes**, updated fonts and correct Arabic font selection.
- **Age ratings on detail pages:** choose from 32 countries; US fallback is labelled and missing ratings stay hidden.
- **Refreshed playback engines, providers and connection security.**
- **MDBList fixes retained:** library syncing, watched indicators and episode-carousel position.

See the [full release notes](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/v1.0.0.26-native/RELEASE_NOTES.md) for the detailed changelog.

## Library, profiles and discovery

- **New search controls (next build):** type with the multilingual controller keyboard, use L1/R1 to change languages, or open the PS5 keyboard with Options. Title suggestions come from your configured searchable catalogues.
- **Optional top menu (next build):** turn on Settings → App & Updates → Top navigation for a dedicated Search tab. All enabled sections stay available. [Quick guide](source/native-app/SEARCH_NAVIGATION.md).

- **Profiles and sync:** up to six profiles, PIN entry, and library/watch progress synced across Nuvio devices.
- **Title actions:** add/remove titles and mark movies or episodes watched. Season/show changes ask for confirmation.
- **Trakt, SIMKL and MDBList:** separate library and watch-progress choices. SIMKL library browsing is read-only.
- **People and trailers:** browse cast/crew biographies and credits, discover related titles and watch supported trailers.
- **Search and sources:** library search, new-episode badges, stream details and saved source preferences.

## Live TV and your own media

- **Thai subtitle compatibility (test fix):** adds a bundled fallback for untagged Thai captions and decoding for Thai Windows-874/TIS-620 files. [Test details](source/native-app/TESTING-ISSUE27.md).
- **Unified TV guide (next build):** browse personal IPTV sources and enabled free channels together. Each channel keeps its own guide, favorites and playback details. Open Settings → Live TV → Unified TV guide. [Details](source/native-app/UNIFIED_TV_GUIDE.md).
- **Optional free live TV (next build):** turn it on in Settings → Live TV, read the disclaimer and select the countries you want. Their channels appear together in one list. No IPTV subscription is needed. Channels are supplied by third parties, so availability and quality vary. Personal playlists stay separate. [Details](source/native-app/FREE_LIVE_TV.md).
- **Live TV:** M3U/M3U8 playlists, Xtream Codes and Stalker/Ministra portals, with groups, search and favorites.
- **USB import:** copy playlists into the selected profile. USB access may need the bundled helper and a compatible payload loader.
- **TV guide:** Now/Next information from your provider or XMLTV.
- **IPTV Library:** browse provider movies/series, search saved catalogs and resume locally.
- **Optional Sports Hub:** follow leagues/teams, hide score spoilers and find possible matching TV channels.
- **Experimental multiview:** four SDR channels at once, with one selected for sound. Requires four provider connections.
- **Media servers:** connect Plex, Jellyfin or Emby.
- **Addons and providers:** Nuvio manifests, supported JavaScript providers, and cached-file access through TorBox/Premiumize.
- **Experimental torrents and Web Drop:** send magnet links or torrent files from a paired phone/PC browser. Enable torrent mode first; it is off by default.

Addons, personal playlists and media access come from your own sources and accounts.
The optional free TV list comes from public third-party sources.
Availability depends on the provider.

## Playback

- **Three player choices:** Native is the default, with optional **EVO Player 0.13.7** and **VLC 3.0.24** engines.
- **Audio and subtitles:** track/language selection, stereo or surround audio, volume/sync controls and subtitle styling.
- **Episode controls:** IntroDB/AniSkip timings, Skip Intro/Recap/Credits and optional next-episode autoplay with an adjustable countdown.
- **Chapters and seeking:** embedded chapter selection and custom seek intervals.
- **Picture settings:** compatible 4K playback, experimental HDR10, HDR-to-SDR conversion and optional 120 Hz output.
- **TVDB episode ordering:** choose supported alternate series orders using your own TVDB API access, while preserving watch-history identity.

Optional engines, HDR and 120 Hz depend on your console, display and media.
Full Dolby Vision and HDR10+ are not supported. See the
[playback limits](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/v1.0.0.26-native/source/native-app/PLAYBACK_QUALITY.md)
for format details.

## Languages, controls and backups

- **Eight menu languages:** English, German, Spanish, French, Arabic, Simplified Chinese, Japanese and Korean. Change **Settings → App & Updates → Language**, then restart. Advanced help may use English.
- **Controls:** DualSense, supported Media Remote/HDMI-CEC buttons and app-local remote remapping. System-intercepted buttons cannot be reassigned.
- **Appearance:** settings search, reduced motion, higher contrast, interface sounds and controller lighting. Console light sync is optional on supported systems.
- **Encrypted backups:** save profile settings and imported USB playlists, up to 16 MiB. Account sessions, watched history and resume are excluded.
- **Google Drive:** optional manual backup sync with conflict review. Requires your own compatible Google app registration; none is bundled.

## Download and install

You need a jailbroken PS5 and a compatible
[ShadowMount+](https://github.com/drakmor/ShadowMountPlus) setup. Follow the
loader's instructions for your firmware.

| Download | Install |
| --- | --- |
| [**Homebrew Store**](https://prosperostore.net/app/PPSA99288/) | Find **Nuvio Max'd** and install the listed build. |
| [**ZIP folder**](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/download/v1.0.0.26-native/PPSA99288.zip) | Extract `PPSA99288/` into a folder scanned by ShadowMount+, such as `/data/homebrew/`. The app should be at `/data/homebrew/PPSA99288/eboot.bin`, without an extra folder level. |
| [**FFPFSC image**](https://github.com/HandshakeHackin/Nuvio-maxd-releases/releases/download/v1.0.0.26-native/NuvioMaxd-1.0.0.26-native.ffpfsc) | Put the image in a homebrew location scanned by ShadowMount+. |

Use one installation format at a time. Close Max'd before updating and keep your
app data. GitHub downloads include matching source and checksums.

After launch:

1. Scan the sign-in QR code and choose a profile, or continue as Guest.
2. Set up your addons, TV sources or media-server connection.
3. Choose something to watch. Adjust playback and language settings as needed.

## Help and credits

[Report a problem](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues)
with your app version, PS5 firmware, install format, playback engine and what
happened. Remove passwords, tokens and private stream URLs from shared logs.

Nuvio Max'd is an unofficial client and is not affiliated with Nuvio, Stremio or
Sony. It builds on [Nuvio protocols](https://github.com/NuvioMedia/NuvioTVSmart),
[Sp9nky's native Stremio port](https://github.com/Sp9nky/unofficial-stremio-ps5-port)
and [BlackBearReloaded's Prospero UI](https://github.com/blackbearreloaded/ps5-homebrew-ui),
with credited EVO/VLC and media libraries.

[Matching source and build guide](https://github.com/HandshakeHackin/Nuvio-maxd-releases/tree/v1.0.0.26-native/source/native-app) ·
[Third-party notices](https://github.com/HandshakeHackin/Nuvio-maxd-releases/blob/v1.0.0.26-native/source/native-app/THIRD_PARTY.md) ·
[GPL-3.0 license](LICENSE) ·
[Optional support on Ko-fi](https://ko-fi.com/handshakehackin)
