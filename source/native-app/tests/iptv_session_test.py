from pathlib import Path
import http.server,threading,subprocess,sys,urllib.parse,json,tempfile
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 offline=False
 no_vod=False
 def log_message(self,*args):pass
 def do_GET(self):
  p=urllib.parse.urlsplit(self.path)
  if p.path.startswith('/control/'):
   Handler.offline=p.path.endswith('/offline');Handler.no_vod=p.path.endswith('/novod');body=b'OK';self.send_response(200);self.end_headers();self.wfile.write(body);return
  if Handler.offline:self.send_error(503);return
  query=urllib.parse.parse_qs(p.query)
  if query.get('username')!=['fixture-user'] or query.get('password')!=['fixture-pass']:self.send_error(403);return
  action=query.get('action',['auth'])[0];seen.append((p.path,action,query.get('category_id'),query.get('series_id')))
  vod=p.path.startswith('/vod/') and not Handler.no_vod
  if action=='auth':body={'user_info':{'auth':1,'status':'Active'}}
  elif action=='get_vod_categories':body=[{'category_id':1,'category_name':'Movies'}] if vod else []
  elif action=='get_series_categories':body=[{'category_id':2,'category_name':'Shows'}] if vod else []
  elif action=='get_vod_streams':body=[{'stream_id':19,'name':'Provider movie','container_extension':'mkv','category_id':'1','direct_source':'https://wrong.invalid/'},{'stream_id':20,'name':'Another movie','category_id':'1'}] if vod else []
  elif action=='get_series':body=[{'series_id':7,'name':'Provider series','category_id':'2'}] if vod else []
  elif action=='get_series_info':
   assert query.get('series_id')==['7']
   body={'episodes':{'1':[{'id':19,'episode_num':1,'title':'Pilot'},{'id':20,'episode_num':2,'title':'Second'}],'2':[{'id':42,'episode_num':1,'title':'New season'}]}}
  else:self.send_error(400);return
  data=json.dumps(body).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
s=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=s.serve_forever,daemon=True).start()
try:
 with tempfile.TemporaryDirectory() as temp:subprocess.run([sys.argv[1],f'http://127.0.0.1:{s.server_port}',temp],check=True,timeout=60)
 assert any(a=='get_series_info' for _,a,_,_ in seen)
 assert all('live' not in a for _,a,_,_ in seen), 'VOD importer fetched live/addon data'
 # Series episode lookup only follows its series list; discovery never loads episodes.
 first=next(i for i,x in enumerate(seen) if x[1]=='get_series_info')
 assert any(x[1]=='get_series' for x in seen[:first])
 print('Synthetic Xtream HTTP fixture: exact account endpoints and on-demand episodes passed')
finally:s.shutdown();s.server_close()
