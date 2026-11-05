"""Collect the installed, lockfile-pinned production package notices deterministically."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'third_party/provider-runtime'
lock = json.loads((root / 'package-lock.json').read_text())
notices = []
for name, entry in sorted(lock['packages'].items()):
    if not name or entry.get('dev'):
        continue
    package = root / name
    if not package.is_dir():
        raise SystemExit('Missing production package: ' + name)
    metadata = json.loads((package / 'package.json').read_text())
    files = sorted(p for p in package.iterdir()
                   if p.is_file() and p.name.lower().startswith(('license', 'copying')))
    if files:
        text = '\n\n'.join(p.read_text() for p in files)
    elif metadata['name'] == 'boolbase' and entry['version'] == '1.0.0':
        # This tarball declares ISC and its author in package.json but omits a license file.
        text = (root / 'license-overrides/boolbase-1.0.0.txt').read_text()
    else:
        raise SystemExit('Missing license notice: ' + name)
    notices.append(metadata['name'] + ' ' + entry['version'] + '\n' + '\n'.join(line.rstrip() for line in text.strip().splitlines()))
(root / 'LICENSES.txt').write_text('\n\n---\n\n'.join(notices) + '\n')
print(f'Collected {len(notices)} production dependency notices')
