"""Reject a link that silently disables the production VLC picture allocator."""
import os,subprocess,sys
nm=os.environ.get('LLVM_NM','llvm-nm')
rows=subprocess.check_output([nm,'--defined-only',sys.argv[1]],text=True).splitlines()
for name in ['vlc_ps5_big_alloc','vlc_ps5_big_free']:
 matches=[r.split() for r in rows if r.split() and r.split()[-1]==name]
 if not matches or not any(len(r)>=3 and int(r[0],16)>0 and r[-2] in ('T','t') for r in matches):
  raise SystemExit('VLC picture hook is disabled or missing: '+name)
print('VLC picture allocator hooks resolve to production functions')
