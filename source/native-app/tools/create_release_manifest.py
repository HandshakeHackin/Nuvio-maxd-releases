"""Sign immutable app files locally. Private signing keys must stay outside Git."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import tempfile

PROJECT = 'HandshakeHackin/Nuvio-maxd-releases'
REQUIRED = {'CREDITS.txt', 'LICENSE', 'THIRD_PARTY.md', 'build-info.json', 'eboot.bin', 'sce_sys/param.json'}
EXCLUDED = {'release-manifest.json', 'release-manifest.sig'}

def create(app, public_key, private_key=None):
    app, public_key = Path(app), Path(public_key)
    files = []
    total = 0
    for path in sorted(app.rglob('*')):
        if path.is_symlink():
            raise ValueError('Release files cannot be symbolic links')
        if path.is_dir():
            continue
        name = path.relative_to(app).as_posix()
        if name in EXCLUDED:
            continue
        if (len(name) > 240 or not name.isascii() or not name.isprintable()
                or '\\' in name or ':' in name or PurePosixPath(name).as_posix() != name
                or any(part in ('', '.', '..') for part in name.split('/'))):
            raise ValueError('Invalid release file path')
        size = path.stat().st_size
        if size > 128 << 20:
            raise ValueError('Release file exceeds verification budget')
        total += size
        if total > 256 << 20:
            raise ValueError('Release exceeds verification budget')
        with path.open('rb') as stream:
            prefix = stream.read(4096).lstrip()
            if prefix.startswith(b'-----BEGIN ') and b'PRIVATE KEY-----' in prefix.splitlines()[0]:
                raise ValueError('Private signing keys cannot be included in app files')
            stream.seek(0)
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        files.append({'path': name, 'size': size, 'sha256': digest})
    # Path ordering compares components, which differs from byte ordering for
    # pairs such as licenses/libass.txt and licenses/libass/README.md.
    files.sort(key=lambda record: record['path'])
    if len(files) > 512 or not REQUIRED <= {f['path'] for f in files}:
        raise ValueError('Release is missing required files or credits')
    if any(f['path'] in REQUIRED and not f['size'] for f in files):
        raise ValueError('Required release files and credits cannot be empty')
    info = json.loads((app / 'build-info.json').read_text())
    if private_key and info.get('sourceDirty') is not False:
        raise ValueError('Official signing requires clean committed source')
    if not any(PROJECT in line for line in (app / 'CREDITS.txt').read_text().splitlines()):
        raise ValueError('Original project attribution is missing')
    commit = info['sourceCommit']
    if private_key and (not isinstance(commit, str) or len(commit) != 40 or any(c not in '0123456789abcdef' for c in commit)):
        raise ValueError('Invalid source commit')
    payload = {'format': 1, 'project': PROJECT, 'titleId': 'PPSA99288',
               'version': info['version'], 'sourceCommit': commit, 'files': files}
    raw = (json.dumps(payload, ensure_ascii=True, sort_keys=True, separators=(',', ':')) + '\n').encode()
    if len(raw) > 128 * 1024:
        raise ValueError('Release manifest exceeds verification budget')
    manifest, signature = app / 'release-manifest.json', app / 'release-manifest.sig'
    manifest.write_bytes(raw)
    if signature.exists():
        signature.unlink()
    if private_key:
        private_key = Path(private_key).resolve(strict=True)
        native_root = Path(__file__).resolve().parents[1]
        repository = next((p for p in native_root.parents if (p / '.git').exists()), native_root.parent)
        if private_key.is_relative_to(repository) or private_key.is_relative_to(app.resolve()):
            raise ValueError('Keep signing keys outside the repository and app')
        if private_key.stat().st_mode & 0o077:
            raise ValueError('Signing key must have owner-only permissions')
        with tempfile.TemporaryDirectory(prefix='nuvio-sign-') as directory:
            derived = Path(directory) / 'public.der'
            subprocess.run(['openssl', 'pkey', '-in', str(private_key), '-pubout', '-outform', 'DER', '-out', str(derived)], check=True, capture_output=True)
            expected = subprocess.check_output(['openssl', 'pkey', '-pubin', '-in', str(public_key), '-outform', 'DER'], stderr=subprocess.DEVNULL)
            if derived.read_bytes() != expected:
                raise ValueError('Signing key does not match the embedded release key')
        subprocess.run(['openssl', 'pkeyutl', '-sign', '-rawin', '-inkey', str(private_key), '-in', str(manifest), '-out', str(signature)], check=True, capture_output=True)
        subprocess.run(['openssl', 'pkeyutl', '-verify', '-pubin', '-inkey', str(public_key), '-rawin', '-in', str(manifest), '-sigfile', str(signature)], check=True, capture_output=True)
    return len(files), bool(private_key)

def verify(app, public_key):
    app, public_key = Path(app), Path(public_key)
    manifest, signature = app / 'release-manifest.json', app / 'release-manifest.sig'
    with manifest.open('rb') as stream:
        raw = stream.read(128 * 1024 + 1)
    if len(raw) > 128 * 1024 or signature.stat().st_size != 64:
        raise ValueError('Invalid release metadata size')
    subprocess.run(['openssl', 'pkeyutl', '-verify', '-pubin', '-inkey', str(public_key), '-rawin', '-in', str(manifest), '-sigfile', str(signature)], check=True, capture_output=True)
    data = json.loads(raw)
    info = json.loads((app / 'build-info.json').read_text())
    if (set(data) != {'format', 'project', 'titleId', 'version', 'sourceCommit', 'files'}
            or type(data['format']) is not int or data['format'] != 1
            or data['project'] != PROJECT or data['titleId'] != 'PPSA99288'
            or data['version'] != info['version'] or data['sourceCommit'] != info['sourceCommit']):
        raise ValueError('Release identity does not match')
    files = data['files']
    if not isinstance(files, list) or not 6 <= len(files) <= 512:
        raise ValueError('Invalid release file count')
    names = [f['path'] for f in files]
    if names != sorted(set(names)) or not REQUIRED <= set(names):
        raise ValueError('Release paths or credits are incomplete')
    actual = set()
    for path in app.rglob('*'):
        if path.is_symlink():
            raise ValueError('Symbolic links are not release files')
        if path.is_file() and path.relative_to(app).as_posix() not in EXCLUDED:
            actual.add(path.relative_to(app).as_posix())
    if actual != set(names):
        raise ValueError('Package inventory differs from signed release')
    total = 0
    for record in files:
        name = record['path']
        if (set(record) != {'path', 'size', 'sha256'} or not isinstance(name, str)
                or len(name) > 240 or not name.isascii() or not name.isprintable()
                or '\\' in name or ':' in name or PurePosixPath(name).as_posix() != name
                or name.startswith('/') or any(p in ('', '.', '..') for p in name.split('/'))
                or type(record['size']) is not int or not 0 <= record['size'] <= 128 << 20):
            raise ValueError('Invalid signed file metadata')
        total += record['size']
        path = app / name
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if total > 256 << 20 or path.stat().st_size != record['size'] or digest != record['sha256']:
            raise ValueError('Release file does not match its signed hash')
    return len(files)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--public-key', type=Path, default=Path(__file__).resolve().parents[1] / 'release-public-key.pem')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        print(f'Release signature and all {verify(args.app, args.public_key)} immutable file hashes verified')
    else:
        count, signed = create(args.app, args.public_key, os.environ.get('NUVIO_RELEASE_SIGNING_KEY'))
        print(f'Release integrity: {count} immutable files; ' + ('Ed25519 signature verified' if signed else 'unsigned development build (offline warning enabled)'))
