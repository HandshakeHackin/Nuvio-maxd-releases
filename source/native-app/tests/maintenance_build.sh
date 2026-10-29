#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASK_BUILD_TMP=$(mktemp -d)
trap 'rm -rf "$TASK_BUILD_TMP"' EXIT
mkdir -p "$TASK_BUILD_TMP/sdk" "$TASK_BUILD_TMP/work"
printf 'original SDK\n' > "$TASK_BUILD_TMP/sdk/sentinel"
check_rejected() {
  if bash "$ROOT/tools/build_maintenance_libraries.sh" "$TASK_BUILD_TMP/sdk" "$1" > "$TASK_BUILD_TMP/result" 2>&1; then
    echo 'Unsafe SDK overlay was accepted' >&2; exit 1
  fi
  [[ "$(cat "$TASK_BUILD_TMP/result")" == *'must not overlap or alias'* ]]
  [ "$(cat "$TASK_BUILD_TMP/sdk/sentinel")" = 'original SDK' ]
  [ ! -d "$TASK_BUILD_TMP/downloads" ]
}
# W/sdk must not accidentally resolve to the base SDK, including symlink aliases.
check_rejected "$TASK_BUILD_TMP"
ln -s "$TASK_BUILD_TMP/sdk" "$TASK_BUILD_TMP/work/sdk"
check_rejected "$TASK_BUILD_TMP/work"
echo 'Maintenance builder refuses overlapping or aliased base/overlay SDKs before any writes'
