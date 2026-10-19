"""Build metadata registration rejects user tokens and malformed credentials."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
cases = [({}, True), ({'tmdb': {'api_key': '1' * 32}}, True),
         ({'tmdb': {'api_key': ''}}, False), ({'tmdb': {'api_key': 'bad\nheader'}}, False),
         ({'tmdb': {'access_token': 'account-token'}}, False),
         ({'tmdb': {'api_key': '1' * 32, 'password': 'private'}}, False)]
for value, accepted in cases:
    with tempfile.TemporaryDirectory() as directory:
        source, target = (Path(directory) / name for name in ('source.json', 'target.json'))
        source.write_text(json.dumps(value))
        result = subprocess.run([sys.executable, str(root / 'tools/validate_provider_applications.py'), str(source), str(target)], capture_output=True)
        assert (result.returncode == 0) == accepted
        assert target.exists() == accepted
print('Six provider registration validation cases passed (values omitted)')
