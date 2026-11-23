#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
npm ci --ignore-scripts --no-audit --no-fund
./node_modules/.bin/esbuild entry.js --bundle --platform=browser --target=es2020 --external:crypto \
 --inject:./polyfills.js --alias:stream=stream-browserify --alias:util=util/ \
 --alias:domutils=./domutils-compat.js --alias:boolbase=./boolbase-compat.js \
 --alias:css-select=./css-select-compat.js --minify --outfile=runtime.js

python3 ../../tools/generate_provider_licenses.py
