#!/usr/bin/env python3
"""Local MediaBrowser fixtures; never contacts a real account or media provider."""
import http.server,json,subprocess,sys,threading,urllib.parse
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args): pass
 def do_GET(self): self.handle_request()
 def do_POST(self): self.handle_request()
 def handle_request(self):
  u=urllib.parse.urlsplit(self.path);q=urllib.parse.parse_qs(u.query);emby=u.path.startswith('/reverse/emby/');base='/reverse/emby' if emby else '/jellyfin';path=u.path[len(base):]
  header=self.headers.get('X-Emby-Authorization' if emby else 'Authorization','')
  assert self.headers.get('X-Maxd-Credential-Origin') is None
  body=json.loads(self.rfile.read(int(self.headers.get('Content-Length','0'))) or '{}')
  status=200;obj={}
  if path=='/System/Info/Public': obj={'ProductName':'Emby Server' if emby else 'Jellyfin','ServerName':'Fixture server'}
  elif path=='/Users/AuthenticateByName':
   assert body=={'Username':'alice','Pw':'test-password'};assert 'Token=' not in header
   obj={'User':{'Id':'user-1','Name':'Alice'},'AccessToken':'private-fixture-token'}
  else:
   assert 'Token="private-fixture-token"' in header, (path,header)
   assert 'password' not in self.path and 'private-fixture-token' not in self.path
   if path=='/redirect':
    self.send_response(302);self.send_header('Location','http://127.0.0.1:1/credential-leak');self.end_headers();return
   if path=='/denied': status=401
   elif path=='/bad-json': self.send_response(200);self.end_headers();self.wfile.write(b'invalid');return
   elif path=='/oversized':
    self.send_response(200);self.end_headers()
    try:self.wfile.write(b'x'*(16*1024*1024+1))
    except (BrokenPipeError,ConnectionResetError):pass
    return
   elif path=='/slow': status=503
   elif path.endswith('/Images/Primary'):
    self.send_response(200);self.end_headers();self.wfile.write(b'image-bytes');return
   elif path.startswith('/Sessions/'):
    if path!='/Sessions/Logout':
     assert body['ItemId']=='movie-1' and body['MediaSourceId']=='source-1' and body['PlaySessionId']=='session-1'
     assert body['PositionTicks']>=0
     seen.append(path)
    status=204
   elif '/PlaybackInfo' in path:
    item=path.split('/')[2];direct=body['EnableDirectPlay'];assert body['UserId']=='user-1'
    assert body['DeviceProfile']['TranscodingProfiles'][0]['Protocol']=='hls'
    if item=='missing': obj={'ErrorCode':'NotAllowed','MediaSources':[]}
    else:obj={'PlaySessionId':'session-1','MediaSources':[{'Id':'source-1','SupportsDirectPlay':direct,'SupportsDirectStream':False,'TranscodingUrl':'http://foreign.invalid/leak.m3u8' if item=='foreign' else ('/Videos/movie-1/master.m3u8?%61pi_key=private-fixture-token&MediaSourceId=source-1' if item=='bare-relative' else base+'/Videos/movie-1/master.m3u8?api_key=private-fixture-token&MediaSourceId=source-1')}]}
   else:
    assert q['UserId']==['user-1'] and q['Limit']==['100']
    if 'Views' in path: items=[{'Id':'library-1','Name':'Movies','Type':'CollectionFolder'}]
    elif '/Shows/' in path:
     assert path=='/Shows/series-1/Episodes';items=[{'Id':'episode-2','Name':'Episode two','Type':'Episode','IndexNumber':2}]
    else:
     assert path==('/Users/user-1/Items/Resume' if emby and 'Resume' in path else '/Users/user-1/Items' if emby else '/UserItems/Resume' if 'Resume' in path else '/Items')
     if q.get('SearchTerm'): assert q['SearchTerm']==['film & title'] and q['StartIndex']==['100']
     items=[{'Id':'movie-1','Name':'Movie','Type':'Movie','UserData':{'PlaybackPositionTicks':250000000},'RunTimeTicks':9000000000}]
     if q.get('ParentId')==['too-many']:items*=101
    obj={'Items':items,'TotalRecordCount':101}
  self.send_response(status);self.send_header('Content-Type','application/json');self.end_headers()
  if status!=204:
   try:self.wfile.write(json.dumps(obj).encode())
   except (BrokenPipeError,ConnectionResetError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 subprocess.run([sys.argv[1],f'http://127.0.0.1:{server.server_port}'],check=True)
 if 'media-session-host' not in sys.argv[1]: assert seen.count('/Sessions/Playing')>=2 and seen.count('/Sessions/Playing/Stopped')>=2
finally:server.shutdown();server.server_close()
