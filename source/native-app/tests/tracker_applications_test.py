"""Public build registrations must never package user sessions or secrets."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Registrations(unittest.TestCase):
    def validate(self, value, accepted):
        with tempfile.TemporaryDirectory() as directory:
            source, target = (Path(directory) / name for name in ('input.json', 'output.json'))
            source.write_text(json.dumps(value))
            result = subprocess.run(
                [sys.executable, str(ROOT / 'tools/validate_tracker_applications.py'), str(source), str(target)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, accepted, result.stderr)
            if accepted:
                self.assertEqual(json.loads(target.read_text()), value)
            else:
                self.assertFalse(target.exists())

    def test_default_owner_registration(self):
        value = json.loads((ROOT / 'app/tracker-applications.json').read_text())
        self.assertTrue(value['trakt']['client_id'])
        self.validate(value, True)

    def test_public_only_overrides(self):
        self.validate({'trakt': {'client_id': 'public-id'}, 'simkl': {'client_id': 'public-id'}}, True)
        self.validate({}, True)

    def test_reject_private_fields(self):
        for field in ('client_secret', 'access_token', 'refresh_token', 'api_key'):
            with self.subTest(field=field):
                self.validate({'trakt': {'client_id': 'public-id', field: 'private-value'}}, False)

    def test_reject_malformed_ids(self):
        for value in ('', 'bad\nheader', None, 42):
            with self.subTest(value=value):
                self.validate({'trakt': {'client_id': value}}, False)


if __name__ == '__main__':
    unittest.main()
