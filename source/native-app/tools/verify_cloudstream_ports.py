"""Verify the bundled GPL provider ports and their preferred source offline."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
vendor = root / 'third_party/cloudstream-ports'
upstream = json.loads((vendor / 'UPSTREAM.json').read_text())
assert upstream['commit'] == 'ab054659edcdfee0599a50af0e219509d835415c'
assert upstream['license'] == 'GPL-3.0'
for name, expected in upstream['filesSha256'].items():
    path = Path(name)
    assert not path.is_absolute() and '..' not in path.parts
    assert hashlib.sha256((vendor / path).read_bytes()).hexdigest() == expected, name
ports = json.loads((vendor / 'PORTS.json').read_text())
assert ports['version'] == '1.0.16' and len(ports['ports']) == 20
assert len({p['id'] for p in ports['ports']}) == 20
lines = ['// Generated from third_party/cloudstream-ports/PORTS.json.', 'inline constexpr Port ports[] = {']
for port in ports['ports']:
    assert port['id'].isalnum() and port['module'].isalnum()
    path = 'providers/' + port['id'] + '.js'
    assert port['sha256'] == upstream['filesSha256'][path]
    types = (1 if 'movie' in port['types'] else 0) | (2 if 'tv' in port['types'] else 0)
    lines.append('    {' + ', '.join(json.dumps(port[k], ensure_ascii=True) for k in ('module', 'id', 'name', 'sha256')) + ', ' + str(types) + '},')
lines.append('};')
assert (root / 'src/cloudstream_ports.inc').read_text() == '\n'.join(lines) + '\n'
assert (vendor / 'src/kellerkino/index.js').is_file()
assert (vendor / 'shared/http.js').is_file()
print('CloudStream ports: 20 scripts, registry hashes, GPL license and preferred upstream source verified')
