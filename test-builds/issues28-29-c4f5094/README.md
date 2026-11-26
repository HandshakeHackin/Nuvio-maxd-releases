**Superseded internal candidate:** use the [corrected 1.0.1.2 test](../issues28-29-6a64471/README.md). These earlier artifacts remain available for provenance.

# Issues #28 / #29: 1.0.1.2 test

Unconfirmed, unsigned test for USB torrent caching and missing provider scripts. Stable 1.0.1.0 remains available.

- [Installable ZIP](NuvioMaxd-1.0.1.2-issues28-29-test.zip)
- [Native image](NuvioMaxd-1.0.1.2-issues28-29-test.ffpfsc)
- [Matching source](NuvioMaxd-1.0.1.2-source.tar.gz)
- [Test instructions and limits](TESTING.md)
- [SHA-256 checksums](SHA256SUMS)

Source: `c4f5094a490bd7be01b6fd341ab7aceac1148abe`, branch `builds/v1.0.1.2`.

Complete native build, focused regressions and ZIP/native-image/source verification passed. [Exact-source hosted CI](https://github.com/HandshakeHackin/Nuvio-maxd-releases/actions/runs/45832039531) is still running; final evidence follows before the issue test requests.

USB caching is optional under Settings → Torrent settings → Cache location. It needs the existing payload loader on port 9021 and keeps settings/metadata internal. Missing/failed USB storage uses bounded RAM for the rolling buffer. Provider recovery refreshes only the configured repository after a missing-script response.

Close the app before installing and preserve saved data and your previous installer. The dismissible **Unverified build** notice is expected. Please keep private media URLs, tokens and account files out of reports.
