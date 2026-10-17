"""Ensure the actual graphics SDK and Payload SDK match recorded build pins."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
pins = json.loads((root / 'dependencies.json').read_text())
sdk, gl = map(Path, sys.argv[1:3])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

assert sha(gl / 'manifest.sha256') == pins['opengl']['manifestSha256'], 'Wrong OpenGL SDK manifest'
for line in (gl / 'manifest.sha256').read_text().splitlines():
    digest, name = line.split('  ', 1)
    path = Path(name)
    assert not path.is_absolute() and '..' not in path.parts
    assert sha(gl / path) == digest, 'OpenGL SDK file differs: ' + name
for name, digest in pins['sdk']['coreFileSha256'].items():
    assert sha(sdk / name) == digest, 'Payload SDK core differs: ' + name
provider = pins['providerRuntime']
assert sha(root / 'third_party/provider-runtime/runtime.js') == provider['bundleSha256'], 'Provider runtime bundle differs'
assert sha(root / 'third_party/provider-runtime/package-lock.json') == provider['packageLockSha256'], 'Provider dependency lock differs'
print(f"Build dependencies: OpenGL {pins['opengl']['version']} and Payload SDK {pins['sdk']['version']} verified")
