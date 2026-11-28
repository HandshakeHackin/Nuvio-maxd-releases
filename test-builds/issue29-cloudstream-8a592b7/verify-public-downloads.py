"""Download the immutable published ZIP/image/source and verify their bodies."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import re
import sys
import tarfile
import urllib.request
import zipfile

commit=sys.argv[1];assert re.fullmatch('[a-f0-9]{40}',commit)
evidence=Path('/workspace/nuvio-cloudstream-evidence')
folder=Path('/workspace/nuvio-cloudstream/test-builds/issue29-cloudstream-8a592b7')
target=evidence/'published-downloads-8a592b7';target.mkdir(exist_ok=True)
source='8a592b79d8261431e8da06f6d5d148a8d7429364'
report=json.loads((folder/'native-validation.json').read_text())
assert report['sourceCommit']==source and report['hostedCi']['conclusion']=='success'
names=['NuvioMaxd-1.0.1.3-issue29-test.zip','NuvioMaxd-1.0.1.3-issue29-test.ffpfsc','NuvioMaxd-1.0.1.3-source.tar.gz']
expected=dict(zip(names,[report['zipSha256'],report['imageSha256'],report['sourceArchiveSha256']]))
base='https://raw.githubusercontent.com/HandshakeHackin/Nuvio-maxd-releases/'+commit+'/test-builds/issue29-cloudstream-8a592b7/'
def download(name):
    size=(folder/name).stat().st_size;digest=hashlib.sha256();count=0
    with urllib.request.urlopen(urllib.request.Request(base+name,headers={'User-Agent':'NuvioMaxd-test-verification'}),timeout=60) as response:
        assert response.status==200 and response.geturl().startswith('https://')
        with (target/name).open('wb') as stream:
            while chunk:=response.read(1024*1024):
                count+=len(chunk);assert count<=size
                digest.update(chunk);stream.write(chunk)
    assert count==size and digest.hexdigest()==expected[name],name
    return {'name':name,'url':base+name,'bytes':count,'sha256':digest.hexdigest(),'httpStatus':200,'downloadVerified':True}
with ThreadPoolExecutor(max_workers=3) as pool:files=list(pool.map(download,names))
with zipfile.ZipFile(target/names[0]) as archive:
    assert archive.testzip() is None
    assert 'PPSA99288/eboot.bin' in archive.namelist()
    info=json.loads(archive.read('PPSA99288/build-info.json'))
    assert info['sourceCommit']==source and not info['sourceDirty']
    assert info['cloudstreamPorts']['bundledPorts']==20 and not info['cloudstreamPorts']['androidBytecodeExecution']
    ports=json.loads(archive.read('PPSA99288/licenses/cloudstream-ports/PORTS.json'))
    for port in ports['ports']:
        assert hashlib.sha256(archive.read('PPSA99288/providers/cloudstream/'+port['id']+'.js')).hexdigest()==port['sha256']
with tarfile.open(target/names[2],'r:gz') as archive:
    meta=json.load(archive.extractfile('NuvioMaxd-1.0.1.3-source/source-archive.json'))
    assert meta['sourceCommit']==source and meta['version']=='1.0.1.3'
    assert len(meta['fileSha256'])==report['sourceArchiveFiles']
receipt={'artifactCommit':commit,'sourceCommit':source,'zipCrcPassed':True,'entryPointChecked':True,
    'nativePortsChecked':20,'androidBytecodeExecution':False,'matchingSourceMetadataChecked':True,'files':files}
(evidence/'public-download-verification.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('All three public HTTP200 downloads match local verified files; ZIP CRC, entry point, all20 port hashes and source metadata checked.')
