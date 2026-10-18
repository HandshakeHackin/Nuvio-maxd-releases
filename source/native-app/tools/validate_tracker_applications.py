"""Copy only registered application credentials, never a user's tracker session."""
import json
import sys
from pathlib import Path

source, destination = map(Path, sys.argv[1:])
data = json.loads(source.read_text())
if not isinstance(data, dict) or set(data) - {'trakt', 'simkl'}:
    raise SystemExit('Tracker app config must contain only trakt and/or simkl application registrations')
for name, app in data.items():
    required = {'client_id', 'client_secret'} if name == 'trakt' else {'client_id'}
    if not isinstance(app, dict) or set(app) != required:
        raise SystemExit('Tracker app config has missing or unsupported fields; account tokens are forbidden')
    if any(not isinstance(v, str) or not v or len(v) > 4096 or any(ord(c) < 33 or ord(c) > 126 for c in v) for v in app.values()):
        raise SystemExit('Invalid tracker application registration')
destination.write_text(json.dumps(data, separators=(',', ':')) + '\n')
print('Tracker application registrations validated (values omitted)')
