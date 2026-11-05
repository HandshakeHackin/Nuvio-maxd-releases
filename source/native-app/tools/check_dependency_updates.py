"""Report newer stable tags on the app's selected release lines; never change pins."""
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
pins = json.loads((root / 'dependencies.json').read_text())
sources = {name.lower(): item for name, item in pins['maintenanceLibraries']['sources'].items()}
checks = [
    ('sdk', 'ps5-payload-dev/sdk', pins['sdk']['version'], None),
    ('opengl', 'LedyBacer/ps5-opengl', pins['opengl']['version'], None),
    ('openssl', 'openssl/openssl', sources['openssl']['version'], (3, 5)),
    ('curl', 'curl/curl', sources['curl']['version'], None),
    ('expat', 'libexpat/libexpat', sources['expat']['version'], None),
    ('libass', 'libass/libass', sources['libass']['version'], None),
    ('fribidi', 'fribidi/fribidi', sources['fribidi']['version'], None),
    ('harfbuzz', 'harfbuzz/harfbuzz', sources['harfbuzz']['version'], None),
    ('libpng', 'pnggroup/libpng', sources['libpng']['version'], None),
    ('libwebp', 'webmproject/libwebp', sources['libwebp']['version'], None),
    ('libxml2', 'GNOME/libxml2', sources['libxml2']['version'], (2, 15)),
    ('ffmpeg', 'FFmpeg/FFmpeg', sources['ffmpeg']['version'], (7, 0)),
    ('json', 'nlohmann/json', pins['json']['version'], None),
]

def version(tag):
    match = re.search(r'(\d+[._]\d+(?:[._]\d+)?)$', tag)
    return tuple(map(int, re.split(r'[._]', match[1]))) if match else None

results = []
for name, repo, current, line in checks:
    # Tag-only projects have no releases/latest. Read all pages, including older
    # supported branches; reject prerelease suffixes rather than sorting text.
    raw = subprocess.check_output(
        ['gh', 'api', f'repos/{repo}/tags?per_page=100', '--paginate'], text=True)
    decoder = json.JSONDecoder()
    versions = []
    while raw.strip():
        page, end = decoder.raw_decode(raw.lstrip())
        raw = raw.lstrip()[end:]
        for tag in page:
            candidate = version(tag['name'])
            if candidate and (line is None or candidate[:len(line)] == line):
                versions.append(candidate)
    if not versions:
        raise SystemExit('No stable tags found: ' + repo)
    latest = max(versions)
    update = latest > version(current)
    results.append({'component': name, 'pinned': current,
                    'latestInSelectedLine': '.'.join(map(str, latest)),
                    'updateAvailable': update, 'url': f'https://github.com/{repo}/tags'})
    if update:
        print(f'::warning title=Dependency update::{name}: {current} → ' + '.'.join(map(str, latest)))
print(json.dumps(results, indent=2))
