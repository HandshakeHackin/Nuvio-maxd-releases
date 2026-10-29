"""Fail closed if a build uses older or unrecorded networking/subtitle libraries."""
from pathlib import Path
import hashlib,json,sys
root=Path(__file__).resolve().parents[1]
pins=json.loads((root/'dependencies.json').read_text())
sdk=Path(sys.argv[1])
maintenance=pins['maintenanceLibraries']
assert hashlib.sha256((root/maintenance['recipe']).read_bytes()).hexdigest()==maintenance['recipeSha256'], 'Maintenance build recipe differs'
for name,digest in maintenance['targetFileSha256'].items():
    assert hashlib.sha256((sdk/name).read_bytes()).hexdigest()==digest, (
        'Maintenance dependency differs: '+name+
        '. Build the pinned overlay with tools/build_maintenance_libraries.sh BASE_SDK WORK_DIRECTORY')
assert hashlib.sha256((root/'third_party/json.hpp').read_bytes()).hexdigest()==pins['json']['sha256'], 'Vendored JSON differs'
print('Maintenance libraries verified: '+', '.join(k+' '+v['version'] for k,v in maintenance['sources'].items())+'; JSON '+pins['json']['version'])
