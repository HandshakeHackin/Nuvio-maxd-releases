# Nuvio Max'd

Nuvio Max'd is an unofficial native Nuvio client for a jailbroken PS5. Sign in
with a QR code, bring in your addons and library, and sync Watch Progress with
your other Nuvio devices. The interface and player run together on the console.

Public releases include `PPSA99288.zip` for folder installs and ProsperoStore,
`NuvioMaxd-<version>-native.ffpfsc` for image installs, a validation report,
`SHA256SUMS` and `NuvioMaxd-<version>-source.tar.gz` with the exact released native
code, dependency pins, license notices and build instructions. The development
repository remains private; the source for released builds is public in those
archives. Extract the source archive and start with its README and
`native-app/README.md`.

For a ZIP install, extract the `PPSA99288/` folder into a `/homebrew/` folder
scanned by ShadowMount+, such as `/data/homebrew/`, with `eboot.bin` directly
inside `PPSA99288/`. Set the folder and its contents to `0777` if your FTP client
changes permissions. For an image install, put the FFPFSC into a scanned
`/homebrew/` folder. Use one format at a time.

Close the app before replacing its old folder or FFPFSC to update. The title
ID stays `PPSA99288`, so existing Nuvio PS5 installs retain their identity.
Keep `/download0/nuvio` to retain login, settings and progress. The console may
keep the old tile name after an update; the app and downloads use Nuvio Max'd.

Settings → App updates checks this public feed. Its bundled helper only handles
FFPFSC installs and needs ShadowMount+ 1.7's public local API, a payload loader
and an unmounted backing image. You never download or launch an ELF yourself.
ZIP installs use manual updates or ProsperoStore once listed.

These builds are experimental. Review each release's console-testing status,
HDR and audio limits. No commercial media, account credentials or preconfigured
premium addons are bundled. Use your own account and addon setup for content
you are authorized to access. Never put account tokens or playable links in
public issues. This project is not affiliated with Nuvio or Sony.
