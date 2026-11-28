# German provider PS5 ports: 1.0.1.3 test

Unconfirmed, unsigned test for issue #29. Close Max'd before installing and keep
your saved data and previous installer. The dismissible **Unverified build**
notice is expected.

1. Open Addons and reload. If needed, add
   `https://raw.githubusercontent.com/Bnyro/GermanProviders/master/repo.json`.
   The old `main` URL is recognized as an alias on this PS5.
2. Select the German Providers card. Enable **KellerKino** and any other ports
   you want. All switches start off; choices and settings stay in this profile.
3. Try the same movie that failed before. Check whether streams appear and
   whether the chosen stream plays. Also test another available provider.
4. Restart the app and check that the selected providers remain enabled.
   Check another profile to confirm its choices are separate.
5. If a source fails to load, its card should remain visible with an error and
   retry/remove options. A previously saved manifest is retained during a
   failed refresh.

There are 20 standalone PS5 ports covering 18 Bnyro modules; Discovery provides
DMAX, TELE 5 and TLC. Seven entries currently have no port: C3TV, EinschaltenIn,
IptvOrg, Kinoger, SpiegelTV, Welt and ZDF. They are labelled unavailable.
This is not a general Android `.cs3` runtime and needs no companion device.

The GPL JavaScript ports are pinned to dw98-git/nuvio-german-providers commit
`ab054659edcdfee0599a50af0e219509d835415c`, derived from Bnyro's providers.
Each bundle's SHA-256 is checked before execution. Original source, license and
provenance are included in the matching source archive. Pinned CloudStream
metadata is limited to 1 MiB and 256 entries with a shared 15-second deadline;
ordinary addon manifests retain their existing 64 MiB limit. Existing provider
request, redirect, cancellation and public-address checks remain in place.

Please report firmware, whether the repository appears, enabled provider names
and any visible error. Leave out credentials, private stream URLs and saved
account files. The previous subtitle and USB-cache test changes are included.
