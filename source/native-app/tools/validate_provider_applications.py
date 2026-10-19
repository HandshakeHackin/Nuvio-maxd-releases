"""Validate owner metadata registration; never log its value."""
import json
from pathlib import Path
import re
import sys

source, destination = map(Path, sys.argv[1:])
data = json.loads(source.read_text())
if not isinstance(data, dict) or set(data) - {'tmdb'}:
    raise SystemExit('Only the owner TMDB application registration is allowed')
if 'tmdb' in data:
    app = data['tmdb']
    if not isinstance(app, dict) or set(app) != {'api_key'} or not isinstance(app['api_key'], str) or not re.fullmatch(r'[a-fA-F0-9]{32}', app['api_key']):
        raise SystemExit('TMDB needs its v3 application API key; account tokens and other fields are forbidden')
destination.write_text(json.dumps(data, separators=(',', ':')) + '\n')
print('Provider application configuration validated (values omitted)')
