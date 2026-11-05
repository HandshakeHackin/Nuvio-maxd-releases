"""Normalize FFmpeg's informational tool paths to its recorded reproducibility prefix.

Only FFMPEG_CONFIGURATION changes. Compilation still executes the selected SDK.
"""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
pins = json.loads((root / 'dependencies.json').read_text())
receipt = json.loads((root / pins['maintenanceLibraries']['artifactRebuild']['receipt']).read_text())
config, sdk = Path(sys.argv[1]), str(Path(sys.argv[2]).resolve())
text = config.read_text()
lines = text.splitlines(keepends=True)
for i, line in enumerate(lines):
    if line.startswith('#define FFMPEG_CONFIGURATION '):
        assert sdk in line, 'Unexpected FFmpeg configuration SDK prefix'
        lines[i] = line.replace(sdk, receipt['sdkPrefix'])
        break
else:
    raise SystemExit('Missing FFMPEG_CONFIGURATION')
config.write_text(''.join(lines))
