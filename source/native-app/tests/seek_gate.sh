#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bash "$ROOT/tests/build_player_host.sh" seek-gate-host tests/seek_gate_host.cpp file seek
"$ROOT/test-output/seek-gate-host" "$ROOT/test-output/lifecycle.mp4"
