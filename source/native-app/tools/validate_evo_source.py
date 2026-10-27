"""Validate the vendored EVO release against its recorded source hashes."""
import hashlib
import json
from pathlib import Path

def validate():
    root = Path(__file__).resolve().parents[1] / 'third_party/evo'
    pin = json.loads((root / 'UPSTREAM.json').read_text())
    assert pin['version'] == '0.13.0'
    assert pin['commit'] == '2db44b5294657b12bf91666154a05f029ddb5aa2'
    for path, expected in pin['fileSha256'].items():
        assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
    print(f"EVO Player {pin['version']}: {len(pin['fileSha256'])} upstream files verified")

if __name__ == '__main__':
    validate()
