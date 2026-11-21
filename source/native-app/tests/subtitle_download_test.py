"""Real HTTPS, slow handshake, redirects, decompression limits and cancellation."""
import gzip
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'test-output/subtitle-download-host'
CAPTION = b'1\n00:00:01,000 --> 00:00:03,000\nCaption\n'


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def setup(self):
        if self.server.handshake_delay:
            time.sleep(self.server.handshake_delay)
        self.request.do_handshake()
        super().setup()

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        if path in ('/redirect', '/unsafe-redirect'):
            self.send_response(302)
            self.send_header('Location', '/caption' if path == '/redirect' else 'file:///private-subtitle-fixture')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if path == '/stall':
            time.sleep(3)
        status = 503 if path == '/error' else 200
        raw = b'x' * (9 * 1024 * 1024) if path == '/oversized' else CAPTION
        body = gzip.compress(raw)
        self.send_response(status)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Encoding', 'gzip')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    handshake_delay = 0

    def get_request(self):
        sock, addr = super().get_request()
        return self.context.wrap_socket(sock, server_side=True,
                                        do_handshake_on_connect=False), addr

    def handle_error(self, *_):
        # Expected for the deliberately untrusted-CA connection.
        pass


with tempfile.TemporaryDirectory(prefix='nuvio-subtitle-https-') as temp:
    temp = Path(temp)
    cert, key = temp / 'cert.pem', temp / 'key.pem'
    wrong_ca = temp / 'wrong-ca.pem'
    for output, key_file in ((cert, key), (wrong_ca, temp / 'wrong-key.pem')):
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-days', '1', '-keyout', str(key_file), '-out', str(output),
                        '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost'],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    server = Server(('127.0.0.1', 0), Handler)
    server.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    server.context.load_cert_chain(cert, key)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    base = f'https://localhost:{server.server_port}'
    env = dict(os.environ, NO_PROXY='localhost,127.0.0.1', no_proxy='localhost,127.0.0.1')
    cases = [('normal', '/redirect'), ('retry-dns', '/caption'),
             ('retry-connect', '/caption'), ('persistent-dns', '/caption'),
             ('pre-cancel', '/caption'), ('cancel-retry', '/caption'),
             ('cancel-transfer', '/stall'), ('oversized', '/oversized'),
             ('http-error', '/error'), ('bad-ca', '/caption'),
             ('bad-redirect', '/unsafe-redirect'),
             ('slow-connect', '/caption')]
    try:
        for mode, path in cases:
            server.handshake_delay = 11 if mode == 'slow-connect' else 0
            ca = str(wrong_ca if mode == 'bad-ca' else cert)
            subprocess.run([str(BINARY), mode, base + path + '?private=redacted', ca],
                           env=env, check=True, timeout=30)
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
