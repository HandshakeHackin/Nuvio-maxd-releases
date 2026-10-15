from pathlib import Path
import json,hashlib,subprocess
REPO='HandshakeHackin/Nuvio-maxd-releases'
TAG='v0.5.27-native-experimental'
PUBLICATION='848e78b'
def api(*a): return json.loads(subprocess.check_output(['gh','api',*a],text=True))
r=api(f'repos/{REPO}/releases/414305914')
assert r['tag_name']==TAG and r['prerelease']
expected=json.loads(Path('manifest.json').read_text())
p=Path('download/NuvioMaxd-0.5.27-native.ffpfsc')
expected[p.name]={'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
assets={a['name']:a for a in r['assets']}
for n,i in expected.items():
 if n in assets:
  assert assets[n]['size']==i['bytes'] and assets[n]['digest']=='sha256:'+i['sha256'],n
 else:
  assert r['draft'],'Refusing to modify an already published release'
  subprocess.run(['gh','release','upload',TAG,str(Path('download')/n),'--repo',REPO],check=True)
r=api(f"repos/{REPO}/releases/{r['id']}")
assets={a['name']:a for a in r['assets']}
assert set(assets)==set(expected)
for n,i in expected.items():
 assert assets[n]['size']==i['bytes'] and assets[n]['state']=='uploaded',n
 assert assets[n]['digest']=='sha256:'+i['sha256'],n
if r['draft']:
 r=api('--method','PATCH',f"repos/{REPO}/releases/{r['id']}",'-F','draft=false','-F','prerelease=true')
assert not r['draft']
print(r['html_url'])
