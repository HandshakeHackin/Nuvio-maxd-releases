"""Two HLS video programs, each with a different external audio group."""
from pathlib import Path
import functools,http.server,subprocess,threading,re,time
from urllib.parse import urlsplit
r=Path(__file__).resolve().parents[1];p=r/'test-output/hls-variants';p.mkdir(parents=True,exist_ok=True)
def ff(args):subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y']+args,check=True)
ff(['-f','lavfi','-i','testsrc2=size=320x180:rate=25','-f','lavfi','-i','sine=frequency=440:sample_rate=48000','-t','12','-c:v','libx264','-preset','ultrafast','-g','50','-bf','0','-c:a','aac',str(p/'source.mp4')])
for name,track in [('low-audio','0:a:0'),('high-audio','0:a:0'),('low-video','0:v:0'),('high-video','0:v:0')]:
 codec=['-vf','scale=160:90','-c:v','libx264','-preset','ultrafast','-g','50','-bf','0'] if name=='low-video' else ['-c','copy']
 ff(['-i',str(p/'source.mp4'),'-map',track]+codec+['-f','hls','-hls_time','2','-hls_playlist_type','vod','-hls_segment_filename',str(p/(name+'-%d.ts')),str(p/(name+'.m3u8'))])
(p/'master.m3u8').write_text('''#EXTM3U
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="low",NAME="English",LANGUAGE="eng",DEFAULT=YES,AUTOSELECT=YES,URI="low-audio.m3u8"
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="high",NAME="Hebrew",LANGUAGE="heb",DEFAULT=YES,AUTOSELECT=YES,URI="high-audio.m3u8"
#EXT-X-STREAM-INF:BANDWIDTH=300000,RESOLUTION=160x90,CODECS="avc1.42c01e,mp4a.40.2",AUDIO="low"
low-video.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=900000,RESOLUTION=320x180,CODECS="avc1.42c01e,mp4a.40.2",AUDIO="high"
high-video.m3u8
''')
seen=[]
class Handler(http.server.SimpleHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  seen.append(self.path)
  if self.path=='/opaque-master':
   data=(p/'master.m3u8').read_bytes();self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
  path=urlsplit(self.path).path
  if path.startswith('/live/'):
   name=path.removeprefix('/live/')
   if name=='master':data=(p/'master.m3u8').read_bytes()
   elif name.endswith('.m3u8') and '/' not in name:
    text=(p/name).read_text()
    data=re.sub(r'([A-Za-z-]+-\d+)\.ts',r'segments/\1?token=fixture',text).encode()
   elif name.startswith('segments/') and re.fullmatch(r'[A-Za-z-]+-\d+',name.split('/')[-1]):
    if name.endswith('low-video-2'):time.sleep(4)
    data=(p/(name.split('/')[-1]+'.ts')).read_bytes()
   else:self.send_error(404);return
   self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
  super().do_GET()
s=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(p)))
threading.Thread(target=s.serve_forever,daemon=True).start()
try:
 result=subprocess.run([str(r/'test-output/hls-variants-host'),f'http://127.0.0.1:{s.server_port}/opaque-master'],text=True,capture_output=True,timeout=50)
 print(result.stdout);print(result.stderr)
 assert result.returncode==0,'HLS rendition pairing/startup regression failed'
 assert any(x.startswith('/low-video-') for x in seen),'no selected video fetched'
 print('External-rendition HLS playback passed with real FFmpeg and production HTTP input')
 result=subprocess.run([str(r/'test-output/hls-variants-host'),f'http://127.0.0.1:{s.server_port}/live/master','--live'],text=True,capture_output=True,timeout=50)
 print(result.stdout);print(result.stderr)
 assert result.returncode==0,'Live HLS extensionless segment or protocol-isolation regression failed'
 assert any(x.startswith('/live/segments/') for x in seen),'no extensionless live segments fetched'
 assert 'stream fell behind' in result.stdout,'live fixture did not exercise buffer recovery'
 print('Live HTTP-only extensionless HLS segments, stall recovery, audio/video playback and local-file/write rejection passed')
finally:s.shutdown();s.server_close()
