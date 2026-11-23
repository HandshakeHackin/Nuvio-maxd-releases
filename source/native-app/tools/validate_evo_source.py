"""Validate the vendored EVO release against its recorded source hashes."""
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

def validate():
    root = Path(__file__).resolve().parents[1] / 'third_party/evo'
    pin = json.loads((root / 'UPSTREAM.json').read_text())
    assert pin['version'] == '0.13.7'
    assert pin['commit'] == '08bcab5de76cb7507ecc0bc3e1d4bca9bea13788'
    for patch in pin.get('localPatches', []):
        assert hashlib.sha256((root / patch['path']).read_bytes()).hexdigest() == patch['sha256'], patch['path']
    for path, expected in pin['fileSha256'].items():
        expected = pin.get('patchedFileSha256', {}).get(path, expected)
        assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
    # Verify that reversing the recorded local patch restores the original release bytes.
    if pin.get('localPatches'):
        with tempfile.TemporaryDirectory() as scratch:
            temp = Path(scratch)
            for path in pin.get('patchedFileSha256', {}):
                target = temp / path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(root / path, target)
            for patch in reversed(pin['localPatches']):
                subprocess.run(['patch', '--batch', '--reverse', '--fuzz=0', '-p1', '-d', str(temp)],
                               input=(root / patch['path']).read_bytes(), check=True, stdout=subprocess.DEVNULL)
            for path in pin.get('patchedFileSha256', {}):
                assert hashlib.sha256((temp / path).read_bytes()).hexdigest() == pin['fileSha256'][path], path
    print(f"EVO Player {pin['version']}: {len(pin['fileSha256'])} release files and recorded local patches verified")

if __name__ == '__main__':
    validate()
