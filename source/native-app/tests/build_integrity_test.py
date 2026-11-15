"""Real Ed25519/SHA-256 tests against the production C++ verifier and producer."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = Path(sys.argv.pop(1))
spec = importlib.util.spec_from_file_location('manifest_producer', ROOT / 'tools/create_release_manifest.py')
producer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(producer)
VERSION = '1.0.0.27-native'

class IntegrityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='nuvio-integrity-fixture-')
        cls.root = Path(cls.directory.name)
        cls.key = cls.root / 'fixture-private.pem'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', str(cls.key)], check=True, capture_output=True)
        cls.key.chmod(0o600)
        cls.public = cls.root / 'fixture-public.pem'
        subprocess.run(['openssl', 'pkey', '-in', str(cls.key), '-pubout', '-out', str(cls.public)], check=True, capture_output=True)
        der = subprocess.check_output(['openssl', 'pkey', '-pubin', '-in', str(cls.public), '-outform', 'DER'], stderr=subprocess.DEVNULL)
        cls.raw_key = cls.root / 'fixture-public.raw'
        cls.raw_key.write_bytes(der[-32:])
    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nuvio-package-')
        self.addCleanup(self.directory.cleanup)
        self.app = Path(self.directory.name) / 'app'
        self.app.mkdir()
        (self.app / 'sce_sys').mkdir()
        (self.app / 'CREDITS.txt').write_text('Nuvio Max\'d\n' + producer.PROJECT + '\nContributors retained.\n')
        (self.app / 'LICENSE').write_text('Fixture GPL-3.0 notice\n')
        (self.app / 'THIRD_PARTY.md').write_text('Fixture third-party notices\n')
        (self.app / 'eboot.bin').write_bytes(b'fixture executable\0' * 100)
        (self.app / 'sce_sys/param.json').write_text('{"titleId":"PPSA99288","titleName":"Nuvio Max\'d"}')
        self.info = {'version': VERSION, 'sourceCommit': 'a' * 40, 'sourceDirty': False}
        self.write_info()
        (self.app / 'asset.bin').write_bytes(bytes(range(256)) * 300)
        self.sign()
    def write_info(self):
        (self.app / 'build-info.json').write_text(json.dumps(self.info))
    def sign(self):
        return producer.create(self.app, self.public, self.key)
    def result(self, version=VERSION, cancel=False):
        command = [str(EXECUTABLE), str(self.app), version, str(self.raw_key)]
        if cancel:
            command.append('cancel')
        response = subprocess.run(command, check=True, capture_output=True)
        self.assertEqual(response.stderr, b'')
        return json.loads(response.stdout)
    def payload(self):
        return json.loads((self.app / 'release-manifest.json').read_text())
    def forged_signed(self, payload):
        manifest = self.app / 'release-manifest.json'
        manifest.write_text(json.dumps(payload, sort_keys=True, separators=(',', ':')))
        subprocess.run(['openssl', 'pkeyutl', '-sign', '-rawin', '-inkey', str(self.key), '-in', str(manifest), '-out', str(self.app / 'release-manifest.sig')], check=True, capture_output=True)
    def test_real_signature_and_every_file(self):
        result = self.result()
        self.assertEqual(result, {'status': 1, 'reason': 0, 'files': 7, 'credits': True})
        self.assertEqual(producer.verify(self.app, self.public), 7)
    def test_nested_notice_paths_use_the_runtime_lexical_order(self):
        (self.app / 'licenses/libass').mkdir(parents=True)
        (self.app / 'licenses/libass/README.md').write_text('Fixture library details')
        (self.app / 'licenses/libass.txt').write_text('Fixture library license')
        self.sign()
        self.assertEqual(self.result(), {'status': 1, 'reason': 0, 'files': 9, 'credits': True})
        self.assertEqual(producer.verify(self.app, self.public), 9)
    def test_original_compiled_public_key_matches_tracked_key(self):
        der = subprocess.check_output(['openssl', 'pkey', '-pubin', '-in', str(ROOT / 'release-public-key.pem'), '-outform', 'DER'], stderr=subprocess.DEVNULL)
        self.assertEqual(subprocess.check_output([str(EXECUTABLE), 'embedded-key']), der[-32:])
    def test_changed_files_branding_and_credits(self):
        for name in ['eboot.bin', 'sce_sys/param.json', 'CREDITS.txt', 'LICENSE', 'THIRD_PARTY.md', 'asset.bin']:
            with self.subTest(file=name):
                path = self.app / name
                original = path.read_bytes()
                path.write_bytes(original + b' changed')
                self.assertEqual(self.result()['status'], 2)
                path.write_bytes(original)
    def test_same_size_changes_and_truncation(self):
        path = self.app / 'asset.bin'
        data = path.read_bytes()
        path.write_bytes(bytes([data[0] ^ 1]) + data[1:])
        self.assertEqual(self.result()['reason'], 5)
        path.write_bytes(data[:-1])
        self.assertEqual(self.result()['reason'], 5)
    def test_missing_credits_and_files(self):
        for name in ['CREDITS.txt', 'asset.bin']:
            path = self.app / name
            data = path.read_bytes()
            path.unlink()
            self.assertEqual(self.result()['reason'], 4)
            path.write_bytes(data)
    def test_forged_manifest_is_rejected_before_files(self):
        path = self.app / 'release-manifest.json'
        raw = path.read_bytes()
        path.write_bytes(raw.replace(b'Nuvio', b'Other'))
        # PROJECT spelling includes lowercase nuvio; ensure an actual change.
        path.write_bytes(raw + b' ')
        result = self.result()
        self.assertEqual((result['status'], result['reason'], result['files']), (2, 2, 0))
    def test_missing_short_and_overlong_signature(self):
        path = self.app / 'release-manifest.sig'
        data = path.read_bytes()
        for raw in [b'', data[:-1], data + b'x']:
            path.write_bytes(raw)
            self.assertEqual(self.result()['reason'], 2)
        path.unlink()
        self.assertEqual(self.result()['status'], 2)
    def test_version_replay_and_project(self):
        self.assertEqual(self.result(version='1.0.0.28-native')['reason'], 3)
        original = self.payload()
        for field, value in [('project', 'another/project'), ('titleId', 'PPSA99999'), ('format', True), ('sourceCommit', '../secret')]:
            data = dict(original)
            data[field] = value
            self.forged_signed(data)
            self.assertEqual(self.result()['reason'], 3)
    def test_required_notices_cannot_be_omitted(self):
        data = self.payload()
        data['files'] = [f for f in data['files'] if f['path'] != 'CREDITS.txt']
        self.forged_signed(data)
        self.assertEqual(self.result()['reason'], 3)
    def test_unsafe_paths_duplicates_and_order(self):
        original = self.payload()
        for path in ['../outside-secret', '/download0/account.json', 'a//b', 'a/./b', 'C:\\private', 'a\x00b']:
            data = json.loads(json.dumps(original))
            data['files'][0]['path'] = path
            self.forged_signed(data)
            result = self.result()
            self.assertEqual((result['reason'], result['files']), (3, 0))
        for rows in [original['files'] + [original['files'][0]], list(reversed(original['files']))]:
            data = dict(original)
            data['files'] = rows
            self.forged_signed(data)
            self.assertEqual(self.result()['reason'], 3)
    def test_count_size_and_hash_budgets(self):
        original = self.payload()
        for value in [-1, 1.5, 129 << 20, 'bad']:
            data = json.loads(json.dumps(original))
            data['files'][0]['size'] = value
            self.forged_signed(data)
            self.assertEqual(self.result()['reason'], 3)
        data = dict(original)
        data['files'] = original['files'] * 74
        self.forged_signed(data)
        self.assertEqual(self.result()['reason'], 3)
        data = json.loads(json.dumps(original))
        for row in data['files']:
            row['size'] = 64 << 20
        self.forged_signed(data)
        self.assertEqual(self.result()['reason'], 3)
        data = json.loads(json.dumps(original))
        data['files'][0]['sha256'] = 'F' * 64
        self.forged_signed(data)
        self.assertEqual(self.result()['reason'], 3)
    def test_oversized_manifest(self):
        (self.app / 'release-manifest.json').write_bytes(b' ' * (128 * 1024 + 1))
        self.assertEqual(self.result()['status'], 2)
    def test_cancellation_prevents_reading(self):
        self.assertEqual(self.result(cancel=True), {'status': 3, 'reason': 0, 'files': 0, 'credits': False})
    def test_deadline_is_bounded_without_reading_files(self):
        raw = subprocess.check_output([str(EXECUTABLE), str(self.app), VERSION, str(self.raw_key), 'deadline'])
        self.assertEqual(json.loads(raw), {'status': 2, 'reason': 6, 'files': 0, 'credits': False})
    def test_save_data_and_unlisted_files_are_outside_the_signed_file_set(self):
        (self.app.parent / 'profile-settings.json').write_text('{"private":"not read"}')
        (self.app / 'extra-not-loaded.txt').write_text('not a signed release file')
        self.assertEqual(self.result()['status'], 1)
    def test_producer_rejects_dirty_signing_missing_credits_and_wrong_key(self):
        self.info['sourceDirty'] = True
        self.write_info()
        with self.assertRaises(ValueError):
            self.sign()
        self.info['sourceDirty'] = False
        self.write_info()
        (self.app / 'CREDITS.txt').unlink()
        with self.assertRaises(ValueError):
            self.sign()
    def test_producer_rejects_key_in_package_and_symlinks(self):
        copied = self.app / 'accidental-private.pem'
        shutil.copy2(self.key, copied)
        with self.assertRaises(ValueError):
            self.sign()
        copied.unlink()
        (self.app / 'outside-link').symlink_to(self.key)
        with self.assertRaises(ValueError):
            self.sign()
    def test_unsigned_development_remains_buildable(self):
        self.info.update(sourceDirty=True, sourceCommit='unavailable')
        self.write_info()
        count, signed = producer.create(self.app, self.public)
        self.assertFalse(signed)
        self.assertEqual(count, 7)
        self.assertEqual(self.result()['status'], 2)
    def test_wrong_signing_and_verification_keys(self):
        with self.assertRaises(ValueError):
            producer.create(self.app, ROOT / 'release-public-key.pem', self.key)
        self.sign()
        original = self.raw_key.read_bytes()
        try:
            self.raw_key.write_bytes(bytes(32))
            result = self.result()
            self.assertEqual((result['status'], result['files']), (2, 0))
        finally:
            self.raw_key.write_bytes(original)

if __name__ == '__main__':
    unittest.main()
