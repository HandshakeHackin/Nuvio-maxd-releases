# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the unmodified updater helper against a local ShadowMount API fixture.
Linux shims redirect only chdir/log-open/PS5 notification; no image is replaced.
"""
import http.server,json,os,pathlib,subprocess,threading,time
root=pathlib.Path(__file__).resolve().parent
state={}
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_POST(self):
  data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
  route=self.path.removeprefix('/api/v1/')
  state['routes'].append(route)
  if route=='version': result={'status':0,'api_version':1,'capabilities':['game_info','unmount_game','list_images','rescan']}
  elif route=='games/info': result={'status':0,'title_id':'PPSA99288','source_type':'image','image_backed':True,'source_available':True,'installed_pkg':False,'path':'/data/homebrew/Nuvio.ffpfsc'}
  elif route=='games/unmount':
   assert data=={'title_id':'PPSA99288'}
   state['attempts']+=1;n=state['attempts'];kind=state['kind']
   result={'status':16 if kind=='busy' and n<3 else 5 if kind=='refused' else 0}
   if kind=='outage' and n<3:result=None
  elif route=='images':
   released=state['kind']!='persistent' and (state['kind']!='delayed' or state['attempts']>=3)
   result={'status':0,'images':[{'path':'/data/homebrew/Nuvio.ffpfsc','source_available':True,'mapped':not released,'mounted':not released}]}
  else: raise AssertionError('Unexpected API mutation: '+route)
  blob=json.dumps(result).encode();self.send_response(200);self.send_header('Content-Length',str(len(blob)));self.end_headers();self.wfile.write(blob)
server=http.server.ThreadingHTTPServer(('127.0.0.1',10101),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
results=[]
try:
 for kind in ['delayed','busy','outage','refused','persistent']:
  state.clear();state.update(kind=kind,attempts=0,routes=[])
  path=root/('helper-'+kind+'.log');env=dict(os.environ,NUVIO_HELPER_TEST_LOG=str(path))
  proc=subprocess.Popen([str(root/'helper-host')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
  begin=time.monotonic()
  for message,expected in [(json.dumps({'protocol':1,'title_id':'PPSA99288','mode':'probe'}),'ready'),('install','closing')]:
   proc.stdin.write(message+'\n');proc.stdin.flush();reply=json.loads(proc.stdout.readline());assert reply['state']==expected,reply
  proc.stdin.write('exit\n');proc.stdin.flush()
  stdout,stderr=proc.communicate(timeout=145)
  elapsed=round(time.monotonic()-begin,1);log=path.read_text()
  success=kind not in ['persistent','refused']
  assert proc.returncode==(0 if success else 1),(kind,proc.returncode,stderr)
  assert ('test passed' in stderr)==success,(kind,stderr)
  assert 'no image replacement' in log and 'image committed' not in log
  assert state['routes'].count('scan')==0
  if kind in ['delayed','busy','outage']:assert state['attempts']>=3,(kind,state)
  if kind=='persistent':assert elapsed>=120 and 'timed out; original retained' in log
  if kind=='refused':assert state['attempts']==1
  item={'case':kind,'passed':True,'seconds':elapsed,'unmountAttempts':state['attempts'],'exitCode':proc.returncode}
  results.append(item);print(json.dumps(item),flush=True)
finally:server.shutdown();server.server_close()
(root/'helper-protocol-results.json').write_text(json.dumps({'tests':results,'allPassed':True,'scope':'Real helper probe protocol on Linux with syscall/log/notification shims and fixture ShadowMount API; not PS5 hardware acceptance'},indent=2)+'\n')
