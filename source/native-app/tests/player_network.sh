#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$ROOT/test-output/player-network"
bash "$ROOT/tests/build_player_host.sh" player-network-host tests/player_network_host.cpp network seek
python3 "$ROOT/tests/player_network_test.py"
