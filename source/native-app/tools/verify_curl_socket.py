"""Verify the native curl socket-open path actually reaches the PS5 shim."""
from pathlib import Path
import os,subprocess,sys
binary=Path(sys.argv[1])
objdump=str(Path(os.environ['LLVM_NM']).with_name('llvm-objdump'))
caller=subprocess.check_output([objdump,'-d','--disassemble-symbols=cf_socket_open',str(binary)],text=True)
shim=subprocess.check_output([objdump,'-d','--disassemble-symbols=__wrap_curlx_nonblock',str(binary)],text=True)
assert '<__wrap_curlx_nonblock>' in caller,'curl socket-open bypasses the PS5 nonblocking shim'
assert '<__wrap_curlx_nonblock>:' in shim,'PS5 nonblocking shim missing'
assert '<__wrap_fcntl>' not in shim,'PS5 shim still depends on fcntl'
print('Native curl socket-open uses the direct PS5 nonblocking shim')
