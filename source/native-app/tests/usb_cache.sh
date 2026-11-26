#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
B="$ROOT/test-output/usb-cache"
mkdir -p "$B"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -DNUVIO_USB_CACHE_HOST_TEST \
    "$ROOT/cache/helper.cpp" -o "$B/helper"
python3 "$ROOT/tests/usb_cache_helper_test.py" "$B/helper"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -DNUVIO_USB_CACHE_HOST_TEST \
    "$ROOT/tests/usb_cache_host.cpp" "$ROOT/gl/torrent_usb_cache.cpp" "$ROOT/gl/update_channel.cpp" -o "$B/client"
"$B/client" "$B/helper"
