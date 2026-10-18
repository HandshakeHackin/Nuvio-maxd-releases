"""Real HLS/custom-AVIO and first Matroska seek regression on local HTTP."""
from pathlib import Path
import http.server,threading,subprocess,functools,time,re,shutil
root=Path(__file__).resolve().parent.parent
fixtures=root/'test-output/player-network'
(fixtures/'captions.srt').write_text('1\n00:00:01,000 --> 00:00:11,000\nEmbedded captions\n')
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','lavfi','-i','testsrc2=size=192x108:rate=24','-f','lavfi','-i','sine=frequency=440:sample_rate=48000','-f','lavfi','-i','sine=frequency=880:sample_rate=48000','-i',str(fixtures/'captions.srt'),'-t','12','-map','0:v','-map','1:a','-map','2:a','-map','3:s','-c:s','mov_text','-metadata:s:s:0','language=eng','-disposition:s:0','default','-c:v','libx264','-preset','ultrafast','-bf','0','-g','48','-c:a','aac',str(fixtures/'fixture.mp4')],check=True)
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(fixtures/'fixture.mp4'),'-map','0','-c','copy','-c:s','srt',str(fixtures/'fixture.mkv')],check=True)
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(fixtures/'fixture.mp4'),'-map','0:v','-map','0:a:0','-c','copy','-f','hls','-hls_time','2','-hls_playlist_type','vod',str(fixtures/'fixture.m3u8')],check=True)
(fixtures/'live.m3u8').write_text((fixtures/'fixture.m3u8').read_text().replace('#EXT-X-PLAYLIST-TYPE:VOD\n','').replace('#EXT-X-ENDLIST\n',''))
nested=fixtures/'nested';nested.mkdir(exist_ok=True)
for f in fixtures.iterdir():
    if f.is_file():shutil.copyfile(f,nested/f.name)
requests=[]
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        requests.append(self.path)
        if self.path=='/watch':
            self.send_response(302);self.send_header('Location','/nested/fixture.m3u8');self.end_headers();return
        if self.path.startswith('/nested/') and self.headers.get('X-Playback-Test')!='required':
            self.send_error(403);return
        if self.path=='/html-error':
            data=b'<!DOCTYPE html><html><body>Source login required</body></html>'
            self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        if self.path=='/stall-headers':
            time.sleep(1)
            self.send_response(200);self.send_header('Content-Length','0');self.end_headers()
            return
        if self.path=='/stall-read':
            start=int(re.search(r'bytes=(\d+)',self.headers.get('Range','bytes=0')).group(1))
            self.send_response(206 if start else 200)
            self.send_header('Content-Length',str(2000000-start))
            if start:self.send_header('Content-Range',f'bytes {start}-1999999/2000000')
            self.end_headers()
            try:
                self.wfile.write(b'x'*(32 if not start else 1000000));self.wfile.flush()
                if not start:time.sleep(1)
            except (BrokenPipeError,ConnectionResetError):pass
            return
        # Range handling exercises Matroska's deferred cues rather than forcing a reopen.
        path=Path(self.translate_path(self.path))
        if path.is_file() and self.headers.get('Range'):
            data=path.read_bytes();start=int(re.search(r'bytes=(\d+)',self.headers['Range']).group(1))
            self.send_response(206);self.send_header('Content-Length',str(max(0,len(data)-start)))
            self.send_header('Content-Range',f'bytes {start}-{len(data)-1}/{len(data)}');self.end_headers()
            try:self.wfile.write(data[start:])
            except (BrokenPipeError,ConnectionResetError):pass
            return
        super().do_GET()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(fixtures)))
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:
    try:
        result=subprocess.run([str(root/'test-output/player-network-host'),f'http://127.0.0.1:{server.server_port}'],text=True,capture_output=True,timeout=60)
    except subprocess.TimeoutExpired as e:
        print(e.stdout,flush=True);print(e.stderr,flush=True);raise
    print(result.stdout);print(result.stderr)
    assert result.returncode==0,'production player/network fixture failed'
    assert any(p.startswith('/nested/') and p.endswith('.ts') for p in requests),'redirected HLS did not preserve its playlist base'
    assert len({p for p in requests if p.endswith('.ts')})>=3,'did not cross HLS segment boundaries'
    assert (result.stdout+result.stderr).count('player: seek reopen')==1,'expected only the deliberately refused Matroska seek to reopen'
    print('HLS crossed multiple custom-AVIO segments; MP4/Matroska seek stayed in the open session.')
finally:server.shutdown();server.server_close()
