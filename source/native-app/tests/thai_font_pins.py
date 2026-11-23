from pathlib import Path
import hashlib,json
from fontTools.ttLib import TTFont
r=Path(__file__).resolve().parents[1]
j=json.loads((r/'dependencies.json').read_text());p=j['subtitleStack']['fallbackPreparation']
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(r/p['recipe'])==p['recipeSha256']
assert sha(r/p['output'])==p['sha256']
assert sha(r/p['notice'])==p['noticeSha256']
f=TTFont(r/p['output']);cmap=f.getBestCmap();union=set()
for name,digest in p['inputs'].items():
 source=r/'app/fonts'/name;assert sha(source)==digest;union.update(TTFont(source).getBestCmap())
assert union<=cmap.keys() and len(union)==p['codePoints']==4021
assert f['head'].unitsPerEm==1000 and f['name'].getDebugName(1)==p['family'] and f['name'].getDebugName(6)==p['postscriptName']
assert all(cmap.get(ord(c)) not in (None,'.notdef') for c in 'ภาษาไทย ทดสอบ น้ำ English Ελληνικά Русский')
print('Thai fallback font: all 4021 input code points retained; original font hashes, recipe, renamed family and bundled OFL notice verified')
