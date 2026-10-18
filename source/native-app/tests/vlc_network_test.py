from pathlib import Path
import functools,http.server,subprocess,threading,time,re,os,shutil
root=Path(__file__).resolve().parent.parent
fixtures=root/'test-output/vlc'
fixtures.mkdir(parents=True,exist_ok=True)
subprocess.run(['bash',str(root/'tests/vlc_fixtures.sh'),str(fixtures)],check=True)
nested=fixtures/'nested';nested.mkdir(exist_ok=True)
for file in fixtures.iterdir():
 if file.is_file():shutil.copyfile(file,nested/file.name)
seen=[]
class Handler(http.server.SimpleHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  seen.append((self.path,self.headers.get('X-Nuvio-VLC-Test')))
  if self.headers.get('X-Nuvio-VLC-Test')!='required':self.send_error(403);return
  if self.path=='/stall-read':
   self.send_response(200);self.send_header('Content-Length','4096');self.end_headers();self.wfile.write(b'x'*32);self.wfile.flush();time.sleep(3);return
  if self.path=='/stall-headers':time.sleep(3);self.send_response(200);self.send_header('Content-Length','0');self.end_headers();return
  if self.path=='/redirect.m3u8':self.send_response(302);self.send_header('Location','/nested/fixture.m3u8');self.end_headers();return
  path=Path(self.translate_path(self.path))
  if path.is_file() and self.headers.get('Range'):
   data=path.read_bytes();start=int(re.search(r'bytes=(\d+)',self.headers['Range']).group(1))
   self.send_response(206);self.send_header('Content-Length',str(max(0,len(data)-start)));self.send_header('Content-Range',f'bytes {start}-{len(data)-1}/{len(data)}');self.end_headers()
   try:self.wfile.write(data[start:])
   except (BrokenPipeError,ConnectionResetError):pass
   return
  super().do_GET()
s=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(fixtures)))
threading.Thread(target=s.serve_forever,daemon=True).start()
e=os.environ.copy()
base=f'http://127.0.0.1:{s.server_port}'
try:
 # Stream diagnostics so a timeout retains the last completed playback stage.
 r=subprocess.run([str(root/'test-output/vlc-host'),str(fixtures/'fixture.mp4'),str(fixtures/'fixture.mkv'),base+'/fixture.mp4',base+'/fixture.mkv',base+'/fixture.m3u8',base+'/redirect.m3u8',base+'/stall-headers',base+'/stall-read'],env=e,text=True,timeout=100)
 print('requests',len(seen),'segments',len({p for p,h in seen if p.endswith('.ts')}),flush=True)
 assert r.returncode==0,'VLC HTTP playback failed'
 assert all(h=='required' for p,h in seen),'private headers were not preserved'
 assert len({p for p,h in seen if p.endswith('.ts')})>=3,'HLS did not cross segments'
 assert any(p.startswith('/nested/') and p.endswith('.ts') for p,h in seen),'redirect lost the relative playlist base'
 print('VLC authenticated HTTP/HLS, seeking, and cancellation passed')
finally:s.shutdown();s.server_close()
