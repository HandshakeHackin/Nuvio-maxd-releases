from pathlib import Path
import hashlib,json,shutil,zipfile,tarfile
VERSION='0.5.27'
SOURCE='549822f9046e94092cc5b81ba3632cd5ad181706'
def digest(b): return hashlib.sha256(b).hexdigest()
d=Path('download'); d.mkdir(exist_ok=True)
for n,i in json.loads(Path('manifest.json').read_text()).items():
 p=Path('payload')/n
 assert p.stat().st_size==i['bytes'] and digest(p.read_bytes())==i['sha256'],n
 shutil.copy2(p,d/n)
v=json.loads((d/'native-validation.json').read_text())
assert v['sourceCommit']==SOURCE and v['version']==VERSION+'-native-experimental'
assert v['containerErrors']==0 and v['containerWarnings']==0 and v['allExtractedFileHashesMatch']
assert not v['hardwareVerified']
assert all(v[k] for k in ('hostConsoleLightingTestsPassed','hostSettingsInputTestsPassed','hostControllerLightbarTestsPassed','sourceArchiveHashesMatch','folderZipHashesMatch'))
image=f'NuvioMaxd-{VERSION}-native.ffpfsc'
with zipfile.ZipFile(d/f'NuvioMaxd-{VERSION}-FFPFSC.zip') as z:
 assert z.testzip() is None
 b=z.read(image)
 assert len(b)==v['imageBytes'] and digest(b)==v['imageSha256']
 (d/image).write_bytes(b)
with zipfile.ZipFile(d/'PPSA99288.zip') as z:
 assert z.testzip() is None
 assert {n.split('/')[0] for n in z.namelist()}=={'PPSA99288'}
 hashes={n.removeprefix('PPSA99288/'):digest(z.read(n)) for n in z.namelist() if not n.endswith('/')}
 assert hashes==v['fileSha256'] and len(hashes)==135
 assert all((i.external_attr>>16)&0o777==0o777 for i in z.infolist())
 param=json.loads(z.read('PPSA99288/sce_sys/param.json'))
 assert param['titleId']=='PPSA99288' and param['contentVersion']=='05.000.027'
 assert z.read('PPSA99288/sce_sys/param.json')==Path('sce_sys/param.json').read_bytes()
 info=json.loads(z.read('PPSA99288/build-info.json'))
 assert info['sourceCommit']==SOURCE
with tarfile.open(d/f'NuvioMaxd-{VERSION}-source.tar.gz') as t:
 prefix=f'NuvioMaxd-{VERSION}-source/'
 m=json.load(t.extractfile(prefix+'source-archive.json'))
 assert m['sourceCommit']==SOURCE and m['version']==VERSION
 assert len(m['fileSha256'])==582
 for n,h in m['fileSha256'].items():
  assert digest(t.extractfile(prefix+n).read())==h,n
  assert digest((Path('source')/n).read_bytes())==h,n
for line in (d/'SHA256SUMS').read_text().splitlines():
 h,n=line.split('  ',1); assert digest((d/n).read_bytes())==h,n
print('Verified all release assets, 135 package files and 582 source hashes; PS5 hardware testing remains pending.')
