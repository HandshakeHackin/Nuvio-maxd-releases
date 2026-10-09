"""Check the previously verified image and its matching source before publishing."""
import hashlib
import json
from pathlib import Path
import sys
import tarfile
import zipfile

root = Path(sys.argv[1])
report = json.loads((root / 'native-validation.json').read_text())
version = report['version'].removesuffix('-native-experimental')
image = root / f'NuvioMaxd-{version}-native.ffpfsc'
assert image.stat().st_size == report['imageBytes']
assert hashlib.sha256(image.read_bytes()).hexdigest() == report['imageSha256']
assert report['containerErrors'] == 0 and report['containerWarnings'] == 0
assert report['allExtractedFileHashesMatch'] is True
package = root / 'PPSA99288.zip'
assert package.stat().st_size == report['zipBytes']
assert hashlib.sha256(package.read_bytes()).hexdigest() == report['zipSha256']
with zipfile.ZipFile(package) as archive:
    assert archive.testzip() is None
    assert set(name.split('/')[0] for name in archive.namelist()) == {'PPSA99288'}
    packaged = {name.removeprefix('PPSA99288/'): hashlib.sha256(archive.read(name)).hexdigest()
                for name in archive.namelist() if not name.endswith('/')}
    assert packaged == report['fileSha256']
    param = json.loads(archive.read('PPSA99288/sce_sys/param.json'))
    assert param['titleId'] == 'PPSA99288'
    assert param['localizedParameters']['en-US']['titleName'] == "Nuvio Max'd"
with tarfile.open(root / f'NuvioMaxd-{version}-source.tar.gz') as source:
    prefix = f'NuvioMaxd-{version}-source/'
    metadata = json.load(source.extractfile(prefix + 'source-archive.json'))
    assert metadata['sourceCommit'] == report['sourceCommit'] and metadata['version'] == version
    for name, digest in metadata['fileSha256'].items():
        assert hashlib.sha256(source.extractfile(prefix + name).read()).hexdigest() == digest
print('Verified image, store ZIP and matching source archive; console status remains as stated in the report.')
