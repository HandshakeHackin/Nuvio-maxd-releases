import http.server, threading, subprocess, sys, gzip, json
from urllib.parse import urlsplit,parse_qs
xml=b"<tv><programme channel='one' start='20261101120000 +0000' stop='20261101130000 +0000'><title>News</title></programme><programme channel='one' start='20261101130000 +0000' stop='20261101140000 +0000'><title>Next</title></programme></tv>"
seen=[]
session=False
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args): pass
 def do_GET(self):
  url=urlsplit(self.path);seen.append(url.path)
  if url.path=='/guide.xml.gz':
   if session:
    import datetime
    now=datetime.datetime.now(datetime.timezone.utc)
    start=(now-datetime.timedelta(minutes=5)).strftime('%Y%m%d%H%M%S +0000');end=(now+datetime.timedelta(minutes=5)).strftime('%Y%m%d%H%M%S +0000')
    body=gzip.compress(f"<tv><programme channel='one' start='{start}' stop='{end}'><title>News</title></programme></tv>".encode())
   else: body=gzip.compress(xml)
  elif url.path=='/bomb.gz': body=gzip.compress(b' '*(33<<20))
  elif url.path=='/player_api.php':
   q=parse_qs(url.query)
   assert q['username']==['fixture user'] and q['password']==['fixture&pass']
   assert q['action']==['get_short_epg'] and q['stream_id'][0].isdigit() and q['limit']==['8']
   body=json.dumps({'epg_listings':[{'title':'TmV3cyAmIHdlYXRoZXI=','start_timestamp':1793534400,'stop_timestamp':1793538000},{'title':'Test','start_timestamp':1793538000,'stop_timestamp':1793541600}]}).encode()
   if session:
    import time
    now=int(time.time());body=json.dumps({'epg_listings':[{'title':'News','start_timestamp':now-60,'stop_timestamp':now+60},{'title':'Next','start_timestamp':now+60,'stop_timestamp':now+120}]}).encode()
  else: self.send_error(404);return
  self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers()
  try: self.wfile.write(body)
  except (BrokenPipeError,ConnectionResetError): pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:
 subprocess.run([sys.argv[1],f'http://127.0.0.1:{server.server_port}'],check=True)
 assert seen==['/guide.xml.gz','/bomb.gz','/player_api.php'],seen
 if len(sys.argv)>2:
  session=True
  subprocess.run([sys.argv[2],f'http://127.0.0.1:{server.server_port}'],check=True)
  assert seen.count('/guide.xml.gz')==2,seen
finally: server.shutdown();server.server_close();thread.join()
