"""Real NetStream requests, seek/retry behavior and injected DNS failures."""
import http.server
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SIZE = 256 << 20
PATTERN = bytes(range(256)) * 4096


class Server(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        value = self.headers.get('Range', '')
        match = re.fullmatch(r'bytes=(\d+)-(\d*)', value)
        if not match:
            self.send_error(400); return
        start = int(match[1]); end = int(match[2]) if match[2] else SIZE - 1
        chunk = bool(match[2])
        with self.server.lock:
            self.server.requests.append((self.path, start, chunk, time.monotonic()))
            count = sum(1 for _, _, c, _ in self.server.requests if c)
        status = 206
        if chunk and self.path == '/outage': status = 503
        if chunk and self.path == '/missing': status = 404
        if chunk and self.path == '/recover' and count <= 2: status = 503
        if status != 206:
            self.send_response(status); self.send_header('Content-Length', '0'); self.end_headers(); return
        self.send_response(206)
        self.send_header('Content-Range', f'bytes {start}-{end}/{SIZE}')
        self.send_header('Content-Length', str(end - start + 1)); self.end_headers()
        remaining = end - start + 1
        try:
            while remaining:
                amount = min(remaining, len(PATTERN))
                offset = start % 256
                data = PATTERN[offset:offset + amount]
                if len(data) < amount: data += PATTERN[:amount - len(data)]
                self.wfile.write(data); remaining -= amount; start += amount
        except (BrokenPipeError, ConnectionResetError):
            pass


class NetworkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Server)
        self.server.lock = threading.Lock(); self.server.requests = []
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True); self.thread.start()

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(); self.temp.cleanup()

    def run_stream(self, path, mode='parallel'):
        run = subprocess.run([str(ROOT / 'test-output/netstream-host'),
            f'http://127.0.0.1:{self.server.server_port}{path}', mode, self.temp.name],
            capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stderr + run.stdout)
        return json.loads(next(line for line in reversed(run.stdout.splitlines()) if line.startswith('{')))

    def test_parallel_reads_and_seeks_outside_window(self):
        result = self.run_stream('/video', 'seek')
        self.assertTrue(result['correct']); self.assertTrue(result['seek_correct']); self.assertEqual(result['error'], '')

    def test_server_outage_has_shared_cooldown_and_bounded_requests(self):
        result = self.run_stream('/outage')
        requests = [r for r in self.server.requests if r[2]]
        self.assertLessEqual(len(requests), 23)  # failure budget plus already-running workers
        self.assertLessEqual(len({r[1] for r in requests}), 6)
        self.assertGreaterEqual(requests[-1][3] - requests[0][3], 3)
        self.assertEqual(result['read'], -5); self.assertEqual(result['error'], 'HTTP 503')

    def test_transient_chunk_failures_recover(self):
        result = self.run_stream('/recover')
        self.assertTrue(result['correct']); self.assertEqual(result['error'], '')

    def test_terminal_http_error_stops_all_workers(self):
        result = self.run_stream('/missing')
        self.assertLessEqual(sum(1 for r in self.server.requests if r[2]), 6)
        self.assertEqual(result['read'], -5); self.assertEqual(result['error'], 'HTTP 404')

    def test_parallel_dns_failure_does_not_walk_entire_file(self):
        started = time.monotonic()
        result = self.run_stream('/dns-parallel')
        self.assertTrue(result['started']); self.assertEqual(result['read'], -5)
        # Three failures plus at most five already-running workers and the
        # original size probe. Serial failures must not back off for minutes.
        self.assertGreater(result['fault_calls'], 1); self.assertLessEqual(result['fault_calls'], 9)
        self.assertLess(time.monotonic() - started, 15)
        self.assertIn('resolve', result['error'].lower())

    def test_single_connection_dns_failure_stops_after_three_attempts(self):
        result = self.run_stream('/dns-single', 'single')
        self.assertFalse(result['started']); self.assertEqual(result['fault_calls'], 3)
        self.assertIn('resolve', result['error'].lower())


if __name__ == '__main__':
    unittest.main(verbosity=2)
