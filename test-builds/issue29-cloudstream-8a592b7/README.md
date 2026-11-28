# Issue #29: 1.0.1.3 test

Unconfirmed, unsigned standalone PS5 ports for Bnyro’s German providers.

- [Installable ZIP](NuvioMaxd-1.0.1.3-issue29-test.zip)
- [Native image](NuvioMaxd-1.0.1.3-issue29-test.ffpfsc)
- [Matching source and GPL notices](NuvioMaxd-1.0.1.3-source.tar.gz)
- [Test instructions and coverage](TESTING.md)
- [SHA-256 checksums](SHA256SUMS)
- [Verification report](native-validation.json)

Source: `8a592b79d8261431e8da06f6d5d148a8d7429364`, branch `builds/v1.0.1.3`.
The complete native app, package/source checks and [both exact-source CI jobs](https://github.com/HandshakeHackin/Nuvio-maxd-releases/actions/runs/46083377209) passed. Console playback still needs tester confirmation.

Reload Addons, select German Providers and enable KellerKino or another available port. Twenty ports are bundled, and seven entries currently have no PS5 port. The app does not execute Android `.cs3` bytecode or require a companion server. Failed sources stay visible with retry/remove options.

Close the app before installing and retain your saved data and previous installer. The dismissible **Unverified build** notice is expected. This test includes the prior subtitle and USB-cache changes; stable remains 1.0.1.0.
