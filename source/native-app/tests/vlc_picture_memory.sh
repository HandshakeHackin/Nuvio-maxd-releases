#!/usr/bin/env bash
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$R/test-output"
${HOST_CC:-cc} -std=c11 -O1 -g -Wall -Wextra -Werror -pthread "$R/tests/vlc_picture_memory_host.c" -o "$R/test-output/vlc-picture-memory-host"
"$R/test-output/vlc-picture-memory-host"
