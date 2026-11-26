import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

helper = sys.argv[1]
base = {'protocol': 1, 'title_id': 'PPSA99288', 'op': 'open', 'mount': 0, 'limit': 1 << 30}
with tempfile.TemporaryDirectory(prefix='nuvio-cache-protocol-') as temp:
    root = Path(temp)
    (root / 'usb0').mkdir()
    env = dict(os.environ, NUVIO_USB_CACHE_TEST_ROOT=temp)
    invalid = [dict(base, mount=-1), dict(base, mount=8), dict(base, limit=(6 << 30) + 1),
               dict(base, limit=-1), dict(base, path='../private'), dict(base, protocol=2),
               dict(base, title_id='OTHER'), dict(base, mount='0'), dict(base, op='delete'), [], None]
    for request in invalid:
        result = subprocess.run([helper], input=json.dumps(request)+'\n', text=True,
                                capture_output=True, env=env, timeout=3)
        assert result.returncode == 1
        assert list(root.iterdir()) == [root / 'usb0']
        assert not list((root / 'usb0').iterdir())
    invalid_io = [{'op': 'write', 'offset': -1, 'bytes': 1},
                  {'op': 'write', 'offset': 1 << 30, 'bytes': 1},
                  {'op': 'write', 'offset': 0, 'bytes': 262145},
                  {'op': 'write', 'offset': 0, 'bytes': -1},
                  {'op': 'write', 'offset': 0, 'bytes': 0},
                  {'op': 'read', 'offset': 0, 'bytes': '1'},
                  {'op': 'write', 'offset': 0, 'bytes': 1, 'path': '/data/private'},
                  {'op': 'delete', 'offset': 0, 'bytes': 1}]
    for request in invalid_io:
        result = subprocess.run([helper], input=json.dumps(base)+'\n'+json.dumps(request)+'\n',
                                text=True, capture_output=True, env=env, timeout=3)
        replies = [json.loads(line) for line in result.stdout.splitlines()]
        assert result.returncode == 1 and replies[0]['kind'] == 'ready' and replies[-1]['code'] == 'invalid-io'
        assert not list((root / 'usb0/.nuvio-maxd-cache').iterdir())
print('USB cache protocol: 11 invalid opens and8 invalid I/O requests rejected; no path input or unrelated file writes')
