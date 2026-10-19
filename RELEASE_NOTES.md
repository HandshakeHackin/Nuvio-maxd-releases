# 0.5.34.2 temporary issue #2 / #7 test build

Based on 0.5.34.1. Public/store release remains 0.5.34; this is a test distribution.
PS5 numeric contentVersion is 05.000.036.

Updater changes:
- Wait up to about two minutes for the backing-image snapshot after a successful runtime unmount. Busy and transient unavailable responses are retried at one-second intervals; explicit terminal failures stop the attempt. Each HTTP call has its own five-second timeout.
- Keep the helper working directory outside /app0. Retain existing digest, source-path, mounted-image, atomic replacement and backup checks.
- Write status-only diagnostics to /data/nuvio-maxd-updater.log, surviving app closure. No account tokens or stream URLs are logged by this helper.
- App updates includes Test updater handoff (closes app). This runs the real loader/close/unmount path without staging or replacing an image, so it can be tested without a newer public release.

ShadowMount persistent_image_mounts=1 intentionally retains backing images. This build does not override that global policy or replace mounted files. If its log still shows mapped=true/mounted=true, set persistent_image_mounts=0 in /data/shadowmount/config.ini and reload ShadowMount before retrying. The helper still refuses if the app remains active or the image cannot be released.

Trakt changes:
- Configured status requires both Trakt application fields, and matching incomplete configurations can pick up the owner's registration.
- Declined, consumed, invalid and expired device codes terminate with actionable guidance; pending and rate-limited responses retain the normal polling behavior.
- Label missing app registration accurately and tell regular users they do not need a developer account. Pairing diagnostics record only provider/status, not codes or credentials.

Limitation: the source and default package have no registered Max'd Trakt client. This is the root cause reported in issue #2; ordinary-account phone pairing cannot be claimed fixed until the owner supplies a valid application registration via NUVIO_TRACKER_APPLICATIONS. Never use another application's secret or ask reporters for account tokens. Compatible connection import and configured-client flows remain available.

Retains all 0.5.34.1 HLS and VLC fixes. Exact PS5 playback remains unverified. The preceding broader libVLC 3.0.24 CI network regression timed out around HLS end-of-stream seeking/replay; this revision does not claim to fix that separate test failure.

Current verification: 150 package files and 725 matching source hashes passed deep FFPFSC/ZIP checks. 38 updater regressions, tracker fixture contracts and five real-helper probe scenarios passed. Focused CI passed on the final source. Broader playback CI is not green: libVLC HLS replay/network times out; an earlier account/transport run also failed native seek sample trim/gate. See the validation receipt for exact CI status; console acceptance and ordinary-account Trakt pairing are not claimed.
