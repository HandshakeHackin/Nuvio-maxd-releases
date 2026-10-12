"""Validate the vendored EVO release against its recorded source hashes."""
import hashlib
import json
from pathlib import Path

def validate():
    root = Path(__file__).resolve().parents[1] / 'third_party/evo'
    pin = json.loads((root / 'UPSTREAM.json').read_text())
    assert pin['version'] == '0.12.0'
    assert pin['commit'] == '9ee9a5420c73b5be448610e5f0a90a29a75592f3'
    for path, expected in pin['fileSha256'].items():
        assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
    print(f"EVO Player {pin['version']}: {len(pin['fileSha256'])} upstream files verified")

if __name__ == '__main__':
    validate()
