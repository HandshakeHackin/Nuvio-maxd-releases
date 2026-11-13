"""Prepare a new isolated SDK/compiler/source cache from checksum-pinned inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import tarfile
import urllib.request
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('work', type=Path)
parser.add_argument('--base-sdk', type=Path)
parser.add_argument('--verify-only', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
pins = json.loads((root / 'dependencies.json').read_text())
work = args.work.resolve()
if args.base_sdk:
    base = args.base_sdk.resolve()
    destination = (work / 'sdk').resolve()
    if (work == base or base in work.parents or work in base.parents or
            destination == base or base in destination.parents or destination in base.parents):
        raise SystemExit('Base SDK and new work directory must not overlap.')
cache = work / 'downloads'
cache.mkdir(parents=True, exist_ok=True)

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def download(name, url, expected):
    path = cache / name
    if not path.exists() and not args.verify_only:
        temporary = path.with_suffix(path.suffix + '.part')
        with urllib.request.urlopen(url, timeout=120) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output, length=1024*1024)
        if digest(temporary) != expected:
            raise ValueError('Downloaded checksum differs: ' + name)
        temporary.rename(path)
    if not path.is_file() or digest(path) != expected:
        raise ValueError('Missing or mismatched pinned archive: ' + name)
    print('Verified ' + name, flush=True)
    return path

sdk_zip = download('ps5-payload-sdk-' + pins['sdk']['version'] + '.zip',
                   pins['sdk']['archiveUrl'], pins['sdk']['archiveSha256'])
pacbrew = pins['sdk']['pacbrewArchive']
pacbrew_archive = download('ps5-payload-dev.tar.gz', pacbrew['url'], pacbrew['sha256'])
llvm = pins['hostToolchain']
llvm_archive = download(llvm['archiveUrl'].rsplit('/', 1)[-1], llvm['archiveUrl'], llvm['archiveSha256'])
sources = []
for item in pins['maintenanceLibraries']['sources'].values():
    sources.append(download(item['archive'], item['url'], item['sha256']))
generator = pins['hostToolchain']['gperf']
sources.append(download('gperf-' + generator['version'] + '.tar.gz', generator['url'], generator['sha256']))
if args.verify_only:
    print('All SDK/compiler/native source archives match the recorded pins.')
    raise SystemExit(0)

sdk = work / 'sdk'
if sdk.exists():
    raise SystemExit('SDK destination already exists; verify it or choose a new work directory.')
if args.base_sdk:
    base = args.base_sdk.resolve()
    if sdk == base or base in sdk.parents or sdk in base.parents:
        raise SystemExit('Base SDK and new work SDK must be separate.')
    # Unchanged files share storage; every overwritten leaf is unlinked first.
    for name, expected in pins['sdk']['coreFileSha256'].items():
        if name not in pins['maintenanceLibraries']['targetFileSha256'] and digest(base / name) != expected:
            raise ValueError('Base SDK core differs: ' + name)
    def clone(source, target):
        try:
            return os.link(source, target)
        except OSError:
            return shutil.copy2(source, target)
    shutil.copytree(base, sdk, copy_function=clone, symlinks=True)
else:
    sdk.mkdir()
    with zipfile.ZipFile(sdk_zip) as archive:
        entry = next(x for x in archive.infolist() if x.filename.endswith('bin/prospero-clang'))
        prefix = entry.filename[:-len('bin/prospero-clang')]
        for member in archive.infolist():
            if not member.filename.startswith(prefix):
                continue
            relative = Path(member.filename[len(prefix):])
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe SDK archive entry')
            path = sdk / relative
            if member.is_dir():
                path.mkdir(parents=True, exist_ok=True)
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            mode = member.external_attr >> 16
            raw = archive.read(member)
            if stat.S_ISLNK(mode):
                target = Path(raw.decode())
                if target.is_absolute():
                    raise ValueError('Absolute SDK link')
                path.symlink_to(target)
            else:
                path.write_bytes(raw)
                path.chmod(stat.S_IMODE(mode) or (0o755 if relative.parts[0] == 'bin' else 0o644))

with tarfile.open(pacbrew_archive) as archive:
    prefix = 'opt/ps5-payload-sdk/target/user/homebrew/'
    for member in archive:
        if not member.name.startswith(prefix):
            continue
        relative = Path(member.name[len('opt/ps5-payload-sdk/'):])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Unsafe PacBrew archive entry')
        path = sdk / relative
        if member.isdir():
            path.mkdir(parents=True, exist_ok=True)
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.parent.resolve().is_relative_to(sdk.resolve()):
            raise ValueError('PacBrew entry traverses a parent link')
        if path.exists() or path.is_symlink():
            path.unlink()
        if member.issym():
            target = Path(member.linkname)
            if target.is_absolute():
                value = str(target)
                if value.startswith('/opt/ps5-payload-sdk/'):
                    target = sdk / value[len('/opt/ps5-payload-sdk/'):]
                elif value.startswith('/user/homebrew/'):
                    target = sdk / 'target' / value.lstrip('/')
                else:
                    raise ValueError('External PacBrew link')
                target = Path(os.path.relpath(target, path.parent))
            path.symlink_to(target)
        elif member.isfile() or member.islnk():
            with archive.extractfile(member) as source, path.open('xb') as output:
                shutil.copyfileobj(source, output)
            path.chmod(member.mode)
        else:
            raise ValueError('Unsupported PacBrew archive entry')

for source in sources:
    with tarfile.open(source) as archive:
        first = archive.next()
        directory = work / Path(first.name).parts[0]
        if not directory.exists():
            archive.extractall(work, filter='data')

import zstandard
destination = work / 'llvm23'
destination.mkdir()
tools = {'clang','clang-23','clang++','clang-cpp','ld.lld','lld','llvm-ar','llvm-ranlib',
         'llvm-nm','llvm-objcopy','llvm-strip','llvm-objdump','llvm-config','llvm-readelf',
         'llvm-readobj','llvm-size','llvm-strings','llvm-link'}
with llvm_archive.open('rb') as raw, zstandard.ZstdDecompressor(max_window_size=2*1024*1024*1024).stream_reader(raw) as stream:
    with tarfile.open(mode='r|', fileobj=stream) as archive:
        for member in archive:
            relative = Path(*Path(member.name).parts[1:])
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe LLVM archive entry')
            dynamic = relative.parent == Path('lib') and relative.name.startswith(('libLLVM','libclang-cpp')) and '.so' in relative.name
            if not ((relative.parent == Path('bin') and relative.name in tools) or str(relative).startswith('lib/clang/23/') or dynamic):
                continue
            if member.isdir():
                continue
            path = destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if member.issym():
                target = Path(member.linkname)
                if target.is_absolute() or '..' in target.parts:
                    raise ValueError('External LLVM link')
                path.symlink_to(target)
            elif member.isfile():
                with archive.extractfile(member) as source, path.open('xb') as output:
                    shutil.copyfileobj(source, output)
                path.chmod(member.mode)
            elif member.islnk():
                target = Path(*Path(member.linkname).parts[1:])
                if target.is_absolute() or '..' in target.parts:
                    raise ValueError('External LLVM hardlink')
                path.symlink_to(destination / target)
            else:
                raise ValueError('Unsupported LLVM archive entry')
receipt = {'sdkArchiveSha256':pins['sdk']['archiveSha256'], 'pacbrewArchiveSha256':pacbrew['sha256'],
           'llvmArchiveSha256':llvm['archiveSha256'], 'originalSdkModified':False}
(work / 'prepared-inputs.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Isolated SDK, compiler and source cache prepared. Build and validate the overlay before packaging.')
