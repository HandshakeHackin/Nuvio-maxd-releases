#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -pthread -DNUVIO_USB_READER_HOST_TEST -I"$R/third_party" "$R/usb/helper.cpp" -o "$R/test-output/usb-reader-helper"
python3 "$R/tests/usb_reader_helper_test.py"
${HOST_CXX:-g++} -std=c++20 -O1 -Wall -Wextra -Werror -Wno-misleading-indentation -pthread -DNUVIO_USB_READER_HOST_TEST -Wl,--wrap=dup,--wrap=fdopendir,--wrap=fstatat,--wrap=openat -I"$R/third_party" "$R/tests/usb_reader_client_host.cpp" "$R/gl/usb_reader.cpp" "$R/gl/update_channel.cpp" "$R/src/live_tv.cpp" $(pkg-config --cflags --libs libcurl) -o "$R/test-output/usb-reader-client"
"$R/test-output/usb-reader-client"
