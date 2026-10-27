#!/usr/bin/env python3
import http.server,json,subprocess,threading,sys,gzip,resource
from urllib.parse import urlsplit,parse_qs
requests=[]
large_channels=gzip.compress(json.dumps([{'stream_id':i,'name':f'Channel {i}',
 'category_id':'1','provider_metadata':{'epg':'x'*512,'nested':[1,2,3]}}
 for i in range(1,60001)]).encode())
large_m3u=('#EXTM3U\n'+''.join(f'#EXTINF:-1 group-title="News" tvg-name="'+('x'*100)+f'",Channel {i}\nhttps://example.test/live/{i}.ts\n' for i in range(1,60001))).encode()
assert len(large_m3u)>8*1024*1024
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  url=urlsplit(self.path);q=parse_qs(url.query);requests.append((url.path,q))
  if url.path=='/redirect':
   self.send_response(302);self.send_header('Location','/lists/main.m3u');self.end_headers();return
  if url.path=='/oversized':
   self.send_response(200);self.send_header('Content-Length',str(128*1024*1024+1));self.end_headers();return
  elif url.path=='/large.m3u':body=large_m3u
  elif url.path=='/large/player_api.php':
   if q.get('action')==['get_live_streams']:
    self.send_response(200);self.send_header('Content-Encoding','gzip');self.send_header('Content-Length',str(len(large_channels)));self.end_headers()
    try:self.wfile.write(large_channels)
    except (BrokenPipeError,ConnectionResetError):pass
    return
   data=[{'category_id':'1','category_name':'News'}] if q.get('action')==['get_live_categories'] else {'user_info':{'auth':1,'status':'Active'}}
   body=json.dumps(data).encode()
  elif url.path=='/lists/main.m3u':body=b'#EXTM3U\n#EXT-X-SESSION-DATA:DATA-ID="com.example.playlist",VALUE="1"\n#EXTINF:-1 group-title="News",News\n../channels/news.m3u8\n'
  elif url.path=='/player_api.php':
   if q.get('username')!=['user &'] or q.get('password')!=['secret/?']:data={'user_info':{'auth':0}}
   elif q.get('action')==['get_live_categories']:data=[{'category_id':1,'category_name':'News'}]
   elif q.get('action')==['get_live_streams']:data=[{'stream_id':1,'name':'News HD','category_id':'1'}]
   else:data={'user_info':{'auth':1,'status':'Active','allowed_output_formats':['m3u8']}}
   body=json.dumps(data).encode()
  else:body=b'not a playlist'
  self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers()
  try:self.wfile.write(body)
  except (BrokenPipeError,ConnectionResetError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 subprocess.run([sys.argv[1],f'http://127.0.0.1:{server.server_port}'],check=True,timeout=60)
 assert any(q.get('action')==['get_live_categories'] for _,q in requests)
 assert any(q.get('action')==['get_live_streams'] for _,q in requests)
 assert all(q.get('action',[None])[0] in (None,'get_live_categories','get_live_streams') for _,q in requests)
 print('60,000-channel HTTP M3U and gzip Xtream imports passed; no VOD requests. Host peak RSS:',resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'KiB')
finally:server.shutdown();server.server_close()
