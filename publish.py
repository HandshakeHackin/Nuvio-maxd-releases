from pathlib import Path
import hashlib
import json
import subprocess

REPO = 'HandshakeHackin/Nuvio-maxd-releases'
TAG = 'v0.5.14-native-experimental'
PUBLICATION = '24550ae475c9613b86998fc344b11333ab6c90f6'

def api(*args):
    return json.loads(subprocess.check_output(['gh', 'api', *args], text=True))

assert not api(f'repos/{REPO}')['private']
matches = [r for r in api(f'repos/{REPO}/releases?per_page=100') if r['tag_name'] == TAG]
assert len(matches) <= 1
if matches:
    release = matches[0]
else:
    request = Path('download/release-request.json')
    request.write_text(json.dumps({
        'tag_name': TAG, 'target_commitish': PUBLICATION,
        'name': "Nuvio Max'd 0.5.14-native-experimental",
        'body': Path('release-notes.md').read_text(), 'draft': True, 'prerelease': True,
    }))
    release = api('--method', 'POST', f'repos/{REPO}/releases', '--input', str(request))
assert release['tag_name'] == TAG and release['target_commitish'] == PUBLICATION
assert release['prerelease']
expected = json.loads(Path('manifest.json').read_text())
image = Path('download/NuvioMaxd-0.5.14-native.ffpfsc')
expected[image.name] = {'bytes': image.stat().st_size, 'sha256': hashlib.sha256(image.read_bytes()).hexdigest()}
if release['draft']:
    subprocess.run(['gh', 'release', 'upload', TAG, '--repo', REPO, '--clobber',
                    *[str(Path('download') / name) for name in expected]], check=True)
release = api(f"repos/{REPO}/releases/{release['id']}")
assets = {a['name']: a for a in release['assets']}
for name, info in expected.items():
    asset = assets[name]
    assert asset['size'] == info['bytes'] and asset['state'] == 'uploaded', name
    assert asset.get('digest') == 'sha256:' + info['sha256'], name
if release['draft']:
    release = api('--method', 'PATCH', f"repos/{REPO}/releases/{release['id']}",
                  '-F', 'draft=false', '-F', 'prerelease=true')
assert not release['draft'] and release['prerelease']
print(release['html_url'])
