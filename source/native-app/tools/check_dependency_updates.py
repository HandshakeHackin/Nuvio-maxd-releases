"""Report upstream stable releases without changing any build dependency pins."""
import json
from pathlib import Path
import re
import subprocess
import urllib.request


def version(tag, component=None):
    # An entire release tag must match: pngmeta_1.11 is not libpng 1.11.
    prefixes = {'miniupnpc': 'miniupnpc_', 'dht': 'dht-'}
    prefix = re.escape(prefixes[component]) if component in prefixes else r'(?:v|n|openssl-|curl-|R_|VER-)?'
    # Numeric recorded pins have no project prefix.
    match = re.fullmatch(r'(?:' + prefix + r')?(\d+(?:[._-]\d+){1,2})', tag)
    if not match:
        return None
    parts = tuple(map(int, re.split(r'[._-]', match[1])))
    parts += (0,) * (3 - len(parts))
    if component == 'xz' and parts[1] % 2:
        return None
    if component == 'fontconfig' and parts[2] >= 90:
        return None
    return parts


def checks(pins):
    sources = {name.lower(): item for name, item in pins['maintenanceLibraries']['sources'].items()}
    result = [
        ('sdk', 'ps5-payload-dev/sdk', pins['sdk']['version'], None),
        ('pacbrew', 'ps5-payload-dev/pacbrew-repo', pins['sdk']['pacbrewVersion'], None),
        ('opengl', 'LedyBacer/ps5-opengl', pins['opengl']['version'], None),
        ('json', 'nlohmann/json', pins['json']['version'], None),
        ('mkpfs', 'PSBrew/MkPFS', pins['mkpfs']['version'], None),
        ('evo', 'sainsaji/EVO-PLAYER-PS5', pins['evoPlayer']['version'], None),
        ('vlc-ps5-port', 'ZiZc3/VLC', pins['vlc']['auditedPortRelease']['version'], None),
    ]
    repositories = {
        'openssl': ('openssl/openssl', version(sources['openssl']['version'])[:2]), 'curl': ('curl/curl', None),
        'expat': ('libexpat/libexpat', None), 'freetype': ('freetype/freetype', None),
        'libass': ('libass/libass', None), 'fribidi': ('fribidi/fribidi', None),
        'harfbuzz': ('harfbuzz/harfbuzz', None), 'libpng': ('pnggroup/libpng', None),
        'libwebp': ('webmproject/libwebp', None), 'libxml2': ('GNOME/libxml2', (2, 15)),
        'ffmpeg': ('FFmpeg/FFmpeg', version(sources['ffmpeg']['version'])[:2]),
        'zlib': ('madler/zlib', None), 'zstd': ('facebook/zstd', None),
        'xz': ('tukaani-project/xz', None), 'libpsl': ('rockdaboot/libpsl', None),
        'fontconfig': ('fontconfig/fontconfig', None),
        'libiconv': ('https://ftp.gnu.org/pub/gnu/libiconv/', None),
        'miniupnpc': ('miniupnp/miniupnp', None), 'dht': ('jech/dht', None),
    }
    for name, (repo, line) in repositories.items():
        result.append((name, repo, sources[name]['version'], line))
    if set(sources) != set(repositories):
        raise ValueError('Maintenance source coverage differs from recorded pins')
    return result


def decode_pages(raw):
    """Read all of gh --paginate's concatenated JSON arrays."""
    decoder = json.JSONDecoder()
    tags = []
    while raw.strip():
        page, end = decoder.raw_decode(raw.lstrip())
        raw = raw.lstrip()[end:]
        if not isinstance(page, list) or any(not isinstance(tag, dict) or not isinstance(tag.get('name'), str) for tag in page):
            raise ValueError('GitHub returned an invalid tag page')
        tags.extend(page)
    return tags


def evaluate(name, repo, current, line, tags):
    pinned = version(current, name)
    if pinned is None:
        raise ValueError('Invalid recorded version: ' + name)
    stable = [(version(tag['name'], name), tag['name']) for tag in tags]
    stable = [row for row in stable if row[0] is not None]
    selected = [row for row in stable if line is None or row[0][:len(line)] == line]
    if not selected:
        raise ValueError('No stable tags found in the selected line: ' + repo)
    latest, selected_tag = max(selected)
    newest, newest_tag = max(stable)
    return {'component': name, 'pinned': current,
            'latestInSelectedLine': '.'.join(map(str, latest)), 'selectedTag': selected_tag,
            'updateAvailable': latest > pinned, 'latestStableTag': newest_tag,
            'newerReleaseOutsideSelectedLine': newest > latest,
            'url': repo if repo.startswith('https://') else f'https://github.com/{repo}/tags'}


def gnu_tags(html):
    return [{'name': name} for name in re.findall(r'href="libiconv-(\d+(?:\.\d+)+)\.tar\.gz"', html)]


def main():
    root = Path(__file__).resolve().parents[1]
    pins = json.loads((root / 'dependencies.json').read_text())
    results = []
    for name, repo, current, line in checks(pins):
        # Tag-only projects have no releases/latest. All pages include older
        # supported branches; an incomplete read must fail rather than say current.
        if repo.startswith('https://'):
            with urllib.request.urlopen(repo, timeout=30) as response:
                data = response.read(1024 * 1024 + 1)
            if len(data) > 1024 * 1024:
                raise ValueError('Official source index exceeds bound')
            tags = gnu_tags(data.decode('utf-8'))
        else:
            raw = subprocess.check_output(['gh', 'api', f'repos/{repo}/tags?per_page=100', '--paginate'], text=True)
            tags = decode_pages(raw)
        result = evaluate(name, repo, current, line, tags)
        results.append(result)
        if result['updateAvailable']:
            print(f'::warning title=Dependency update::{name}: {current} → {result["latestInSelectedLine"]}; review compatibility before changing pins')
        if result['newerReleaseOutsideSelectedLine']:
            print(f'::notice title=Upstream release outside selected line::{name}: {result["latestStableTag"]}; retained selected build line')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
