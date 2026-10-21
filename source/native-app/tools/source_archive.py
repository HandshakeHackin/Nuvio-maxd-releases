"""Export the matching native source, public account config and build metadata."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('commit')
parser.add_argument('output', type=Path)
parser.add_argument('--vlc-downloads', type=Path, help='Pinned VLC/libdvbpsi source archive directory')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', args.commit], text=True).strip()
build = subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':native-app/tools/build_info.py'], text=True)
version = re.search(r"'version': '([0-9.]+)-native(?:-experimental)?'", build).group(1)
prefix = f'NuvioMaxd-{version}-source/'
archive = subprocess.check_output(['git', '-C', str(repo), 'archive', '--format=tar', commit,
                                   'README.md', 'native-app', 'ps5/account-config.json',
                                   'ps5/GITHUB.md', '.github/workflows/native-checks.yml'])
deps = json.loads(subprocess.check_output(['git','-C',str(repo),'show',commit+':native-app/dependencies.json'],text=True))
vendor_sources=[]
if 'vlc' in deps:
    if args.vlc_downloads is None:args.vlc_downloads=Path(os.environ.get('VLC_BUILD_ROOT',str(repo/'native-app/build/vlc')))/'downloads'
    provenance=json.loads(subprocess.check_output(['git','-C',str(repo),'show',commit+':native-app/third_party/vlc/UPSTREAM.json'],text=True))
    for pin in [provenance]+[provenance[k] for k in ('libdvbpsi','libebml','libmatroska','utf8cpp') if k in provenance]:
        file=args.vlc_downloads/pin['sourceUrl'].rsplit('/',1)[1]
        if hashlib.sha256(file.read_bytes()).hexdigest()!=pin['sourceSha256']:raise SystemExit('VLC source digest mismatch: '+file.name)
        vendor_sources.append(file)
hashes = {}
with tarfile.open(fileobj=io.BytesIO(archive)) as original, tarfile.open(args.output, 'w:gz') as out:
    for member in original:
        name = member.name
        data = original.extractfile(member).read() if member.isfile() else None
        if data is not None:
            hashes[name] = hashlib.sha256(data).hexdigest()
        member.name = prefix + name
        out.addfile(member, io.BytesIO(data) if data is not None else None)
    for file in vendor_sources:
        data=file.read_bytes();name='vendor-sources/'+file.name
        hashes[name]=hashlib.sha256(data).hexdigest()
        member=tarfile.TarInfo(prefix+name);member.size=len(data);member.mode=0o644
        out.addfile(member,io.BytesIO(data))
    data = (json.dumps({'sourceCommit': commit, 'version': version, 'fileSha256': hashes}, indent=2) + '\n').encode()
    member = tarfile.TarInfo(prefix + 'source-archive.json')
    member.size = len(data)
    member.mode = 0o644
    out.addfile(member, io.BytesIO(data))
print(f'{args.output.name}: version {version}, source {commit}, {len(hashes)} files')
