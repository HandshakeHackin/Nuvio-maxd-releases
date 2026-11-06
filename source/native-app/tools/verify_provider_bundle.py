"""Reject dependency-only updates that leave the shipped provider bundle unverified."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
runtime = root / 'third_party/provider-runtime'
pins = json.loads((root / 'dependencies.json').read_text())['providerRuntime']
failures = []
for name, key in [('runtime.js', 'bundleSha256'), ('package-lock.json', 'packageLockSha256')]:
    digest = hashlib.sha256((runtime / name).read_bytes()).hexdigest()
    if digest != pins[key]:
        failures.append(name)
manifest = json.loads((runtime / 'package.json').read_text())
lock = json.loads((runtime / 'package-lock.json').read_text())
if manifest['dependencies'] != lock['packages']['']['dependencies']:
    failures.append('package manifest/lock mismatch')
if failures:
    raise SystemExit('Provider pins differ: ' + ', '.join(failures) +
                     '. Rebuild runtime.js and notices, review the dependency graph, and update dependencies.json.')
print('Provider bundle and dependency lock match the recorded build pins')
