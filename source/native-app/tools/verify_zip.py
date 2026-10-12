"""Verify the store ZIP against every file in the validated image."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('report', type=Path)
args = parser.parse_args()
report = json.loads(args.report.read_text())
with zipfile.ZipFile(args.package) as archive:
    assert archive.testzip() is None
    for entry in archive.infolist():
        parts = PurePosixPath(entry.filename).parts
        assert parts and parts[0] == 'PPSA99288'
        assert '..' not in parts and not entry.filename.startswith('/')
        assert '\\' not in entry.filename and not entry.flag_bits & 1
        assert not stat.S_ISLNK(entry.external_attr >> 16)
        assert (entry.external_attr >> 16) & 0o777 == 0o777
    files = {entry.filename.removeprefix('PPSA99288/'):
             hashlib.sha256(archive.read(entry)).hexdigest()
             for entry in archive.infolist() if not entry.is_dir()}
    assert files == report['fileSha256'], 'ZIP differs from validated image'
    param = json.loads(archive.read('PPSA99288/sce_sys/param.json'))
    assert param['titleId'] == 'PPSA99288'
    assert param['localizedParameters']['en-US']['titleName'] == "Nuvio Max'd"
report.update(zip=args.package.name, zipBytes=args.package.stat().st_size,
              zipSha256=hashlib.sha256(args.package.read_bytes()).hexdigest(),
              zipFilesMatchImage=True, zipPermissions='0777')
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(f'Verified store ZIP: {len(files)} files match the image; no unsafe entries; permissions 0777')
