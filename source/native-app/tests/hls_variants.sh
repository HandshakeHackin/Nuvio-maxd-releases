#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output/player-network"
bash "$ROOT/tests/build_player_host.sh" hls-variants-host tests/hls_variants_host.cpp network none
python3 "$ROOT/tests/hls_variants_test.py"
