"""A private-source HTTP fixture; production playlist downloads use the real transport."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
import subprocess
import sys
import threading

class Playlist(BaseHTTPRequestHandler):
    calls = 0
    def log_message(self, *_):
        pass
    def do_GET(self):
        assert self.path == "/fixture.m3u?token=private-fixture"
        type(self).calls += 1
        if type(self).calls == 3:
            self.send_error(503)
            return
        body = '#EXTM3U url-tvg="https://epg.invalid/fixture.xml"\n#EXTINF:-1 tvg-id="one" group-title="News",News One\nhttps://streams.invalid/one.m3u8\n'
        if type(self).calls > 1:
            body += '#EXTINF:-1 tvg-id="two" group-title="News",News Two\nhttps://streams.invalid/two.m3u8\n'
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "audio/x-mpegurl")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

server = ThreadingHTTPServer(("127.0.0.1", 0), Playlist)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
env = dict(os.environ, TASK_PLAYLIST_FIXTURE_URL=f"http://127.0.0.1:{server.server_port}/fixture.m3u?token=private-fixture")
try:
    subprocess.run(sys.argv[1:], env=env, check=True)
    assert Playlist.calls >= 3
finally:
    server.shutdown()
    server.server_close()
    thread.join()
