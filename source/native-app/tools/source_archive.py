"""Export the matching native source, public account config and build metadata."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('commit')
parser.add_argument('output', type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', args.commit], text=True).strip()
build = subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':native-app/tools/build_info.py'], text=True)
version = re.search(r"'version': '([0-9.]+)-native-experimental'", build).group(1)
prefix = f'NuvioMaxd-{version}-source/'
archive = subprocess.check_output(['git', '-C', str(repo), 'archive', '--format=tar', commit,
                                   'README.md', 'native-app', 'ps5/account-config.json',
                                   'ps5/GITHUB.md', '.github/workflows/native-checks.yml'])
hashes = {}
with tarfile.open(fileobj=io.BytesIO(archive)) as original, tarfile.open(args.output, 'w:gz') as out:
    for member in original:
        name = member.name
        data = original.extractfile(member).read() if member.isfile() else None
        if data is not None:
            hashes[name] = hashlib.sha256(data).hexdigest()
        member.name = prefix + name
        out.addfile(member, io.BytesIO(data) if data is not None else None)
    data = (json.dumps({'sourceCommit': commit, 'version': version, 'fileSha256': hashes}, indent=2) + '\n').encode()
    member = tarfile.TarInfo(prefix + 'source-archive.json')
    member.size = len(data)
    member.mode = 0o644
    out.addfile(member, io.BytesIO(data))
print(f'{args.output.name}: version {version}, source {commit}, {len(hashes)} files')
