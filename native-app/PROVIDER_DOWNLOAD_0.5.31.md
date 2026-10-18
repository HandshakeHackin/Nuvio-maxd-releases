# Provider download investigation — local 0.5.31

Report: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/3#issuecomment-6249405789

The reporter confirmed 0.5.30, firmware 12.20, Yoru's Repo and only 4KHDHub
enabled. Their log contains `status=0` and `Unsupported protocol (curl 1)` at
`provider: script download failed`, before downloading/executing the script.
The public manifest uses `providers/4khdhub.js`, resolved relative to its HTTPS
manifest. The download error by itself does not prove an unsupported URL scheme.

## Mechanism and fix

curl 8.18.0's `lib/cf-socket.c` skips SOCK_NONBLOCK at socket creation when an
OPENSOCKETFUNCTION callback is installed. After SOCKOPTFUNCTION, it calls
`curlx_nonblock`; a failure returns CURLE_UNSUPPORTED_PROTOCOL. Our provider
client installs this callback to reject private/reserved destinations after DNS
resolution, unlike ordinary app downloads.

The exact PacBrew/PS5 SDK object uses fcntl(F_GETFL/F_SETFL). The existing title
shim tries a SO_NBIO readback only after EINVAL; that readback or another sandbox
fcntl refusal can still fail even when writing SO_NBIO is supported. Thus the
shared SOCKOPTFUNCTION setting does not eliminate curl's second mode-setting
step. The linked 0.5.31 binary was inspected to confirm the old SDK function
called our fcntl shim.

The fix wraps the pinned `curlx_nonblock` with a direct
`setsockopt(SOL_SOCKET, 0x1200 /* SO_NBIO */, ...)`, supporting enable and disable.
A failed setter remains a failure, retains errno and emits a numeric diagnostic.
It does not remove provider address validation, allow local URLs, disable TLS
checks, extend timeouts, or weaken redirect/cancellation/response-size limits.
The SDK curl archive is now hash-checked because this shim uses its internal API.

Reference source:
https://github.com/curl/curl/blob/curl-8_18_0/lib/cf-socket.c
https://github.com/curl/curl/blob/curl-8_18_0/lib/curlx/nonblock.c

## Verification and limits

`tests/curl_socket.sh` compiles the production PS5 shim and links the actual SDK
`libcurl_la-nonblock.o` into a host fixture. With fcntl and SO_NBIO readback
refused, the old function fails; the wrapped path enables/disables nonblocking
mode without either read operation. Setter failures still fail. The fixture
uses the console ABI/constants rather than substituting the host curl build.

`tests/providers.sh` covers script download and execution, relative URL handling,
HTTP errors, timeout redaction, HTTPS downgrade rejection and private-address
blocking. A native link check verifies curl's socket-open code calls the wrapper.

These are code-path and simulated-sandbox results, not a confirmed reproduction
of the reporter's firmware internals. The affected PS5 must retry the same
provider/title. Other provider-specific APIs, site challenges and stream lookup
failures can remain after script downloads work. This is a targeted candidate
fix for issue #3, not a universal provider compatibility claim.
