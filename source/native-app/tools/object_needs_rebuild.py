"""Check compiler-generated header dependencies and the exact compile flags."""
from pathlib import Path
import shlex,sys
obj,dep,stamp=map(Path,sys.argv[1:4]); wanted=sys.argv[4]
if not obj.is_file() or not dep.is_file() or not stamp.is_file() or stamp.read_text().strip()!=wanted:
    raise SystemExit(0)
try:
    text=dep.read_text().replace("\\\n", " ")
    files=shlex.split(text.split(":",1)[1])
    changed=not files or any(not Path(f).is_file() or Path(f).stat().st_mtime_ns>obj.stat().st_mtime_ns for f in files)
except (OSError,ValueError):
    changed=True
raise SystemExit(0 if changed else 1)
