# Nuvio Max’d 0.5.30 — profiles and provider fixes

## Added

- Choose, import or create up to six profiles on one Nuvio account. Open **Settings → Account & Services → Profiles** to switch after stopping playback.
- Separate watch progress, viewing history, addon/plugin repositories and local provider switches per profile. Repository additions/removals and watched history sync for the selected profile; inherited primary sources remain read-only here.
- Account-backed profile PIN checks and migration that keeps guest/different-account data separate.

## Fixed and investigated

- Fixed long provider lists scrolling off-screen. The selected row stays visible, a range indicator shows your position, and toggling a provider keeps the menu open at that row.
- **Possible fix for provider script download failures:** provider requests now share the regular HTTP client’s IPv4 setup. Failures identify the provider and HTTP/curl error instead of displaying only “Could not download provider script”. This does not confirm that every “No playable streams returned” failure is resolved.

## Related reports

- [Issue #3 — No playable streams / provider script download and scrolling](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3): scrolling is corrected in code and host previews; the download change needs confirmation on the reporting PS5. Keep this report open for testing.
- [Issue #4 — Multiple profiles](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/4): profile selection, creation and scoped source/history sync are implemented. Real-account, PIN and cross-device testing are still required.

## Try it

Add a raw `manifest.json` under **Settings → Addons → Add addon or plugin URL**, then open the repository on **Addons** and enable one provider first. Providers start disabled. Cross toggles a provider; Circle closes the list. If downloads still fail, share the `provider: script download failed` line, app version, provider and title, excluding account tokens and stream URLs.

Individual scraper enable switches stay local to each profile; addon/plugin repository lists sync. Android DEX/CS3 plugins are not supported. Profile selection/PIN verification needs a network connection.

## Validation and limits

69 account/HTTPS/sync tests, provider download/runtime and scrolling tests, rendered UI checks, native compilation and package verification pass. All 412 imports resolve, and all 145 extracted files match the ZIP. This remains an experimental release: PS5 and real-account testing are outstanding. Native remains the default player; EVO and VLC remain optional. No new HDR, decoder, graphics-driver or audio fix is claimed.

Preserve your app data when updating. Downloads include the ZIP, FFPFSC, matching source and verification evidence.
