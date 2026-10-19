# Max’d Trakt callback hosting

Dedicated GitHub Pages branch for the HTTPS registration address:
https://handshakehackin.github.io/Nuvio-maxd-releases/trakt/callback/

The current TV/device-code flow does not redirect to this address. The page is a
reserved, owned HTTPS endpoint and does not implement authorization-code/PKCE
login. It does not collect, display, store, forward or exchange OAuth credentials.
It removes query and fragment values from the visible URL without reading them.
Do not claim browser-based login works until a complete flow is implemented.

No analytics, external assets, build dependencies or app artifacts are deployed.
This branch does not change the main app branch, release assets or store listing.
