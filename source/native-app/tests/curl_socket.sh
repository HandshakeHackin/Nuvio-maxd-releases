#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
SDK=${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK to the pinned SDK}
B="$ROOT/test-output/curl-socket"
mkdir -p "$B"
# Do not replace this with the host curl object: its ABI/configuration differs.
"$SDK/bin/prospero-ar" x --output="$B" "$SDK/target/user/homebrew/lib/libcurl.a" libcurl_la-nonblock.o
"$SDK/bin/prospero-clang" -O1 -ffunction-sections -fdata-sections -I"$SDK/target/user/homebrew/include" \
    -c "$ROOT/native/console_curl.c" -o "$B/console_curl.o"
# Localize unrelated console exports so host libc cannot retain its DNS shims.
"$SDK/bin/prospero-objcopy" --keep-global-symbol=__wrap_curlx_nonblock --keep-global-symbol=__wrap_fcntl "$B/console_curl.o"
${HOST_CXX:-g++} -O1 -Wall -Wextra -Werror "$ROOT/tests/curl_socket_host.cpp" \
    "$B/console_curl.o" "$B/libcurl_la-nonblock.o" \
    -Wl,--gc-sections,--wrap=fcntl,--wrap=curlx_nonblock -o "$B/curl-socket-host"
"$B/curl-socket-host"
