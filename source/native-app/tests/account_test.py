"""Exercises production account and curl code against a local HTTPS backend."""
import http.server
from email.utils import formatdate
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time
import unittest
from urllib.parse import parse_qs

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'test-output/account-host'


class Backend(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.respond(None)

    def do_POST(self):
        raw = self.rfile.read(int(self.headers['Content-Length']))
        self.respond(parse_qs(raw.decode()) if self.headers.get('Content-Type', '').startswith('application/x-www-form-urlencoded') else json.loads(raw))

    def respond(self, body):
        state = self.server.state
        state['calls'].append((self.path, body, self.headers.get('Authorization')))
        if self.path.startswith(('/auth/', '/rest/', '/functions/')) and self.headers.get('apikey') != 'public-test-key':
            self.send_json(403, {}); return
        path = self.path
        pathname = path.split('?', 1)[0]
        if path == '/update/redirect':
            self.send_response(302)
            self.send_header('Location', 'http://127.0.0.1:1/never-send-release-data')
            self.end_headers()
            return
        if path == '/auth/v1/signup':
            assert body == {'data': {'tv_client': 'ps5'}}
            self.send_json(200, {'access_token': 'anonymous'})
        elif path.endswith('start_device_login_session') or path.endswith('start_tv_login_session'):
            assert self.headers['Authorization'] == 'Bearer anonymous'
            assert len(body['p_device_nonce']) == 64
            if state.get('legacy') and (path.endswith('start_device_login_session') or 'p_device_name' in body):
                self.send_json(404, {'code': 'PGRST202'}); return
            state['nonce'] = body['p_device_nonce']
            session = {'device_code': 'private-device-code', 'user_code': 'ABCD-EF',
                'verification_uri': 'https://nuvio.tv/link',
                'verification_uri_complete': 'https://nuvio.tv/link?code=ABCD-EF',
                'poll_interval_seconds': 5, 'expires_in': 600}
            for key, value in state.get('pairing_fields', {}).items():
                if value is None:
                    session.pop(key, None)
                else:
                    session[key] = value
            self.send_json(200, [session])
        elif path.endswith('poll_tv_login_session'):
            assert body == {'p_code': 'private-device-code', 'p_device_nonce': state['nonce']}
            status = state.get('pairing_status', 'approved' if state.get('approved') else 'pending')
            self.send_json(200, [{'status': status}])
        elif path.endswith('tv-logins-exchange'):
            assert body == {'code': 'private-device-code', 'device_nonce': state['nonce']}
            self.send_json(200, {'access_token': 'access-first', 'refresh_token': 'refresh-first',
                'expires_at': state.get('server_epoch', time.time()) + (-100 if state.get('expired') else 3600),
                'user': {'is_anonymous': bool(state.get('anonymous_exchange'))}})
        elif path == '/auth/v1/token?grant_type=refresh_token':
            assert body == {'refresh_token': state.get('expected_refresh', 'refresh-first')}
            if state.get('transient'):
                self.send_json(503, {}); return
            if state.get('revoked'):
                self.send_json(400, {'error_code': 'refresh_token_not_found'}); return
            result = {'access_token': 'access-refreshed', 'expires_at': state.get('server_epoch', time.time()) + 3600}
            if not state.get('omit_refresh'):
                result['refresh_token'] = 'refresh-rotated'
            self.send_json(200, result)
        elif path == '/auth/v1/user':
            assert self.headers['Authorization'] in ('Bearer access-first', 'Bearer access-refreshed')
            self.send_json(200, {'id': 'owner-id', 'email': 'test@example.invalid', 'is_anonymous': False})
        elif path.endswith('get_sync_owner'):
            self.send_json(200, 'owner-id')
        elif path == '/provider/form':
            assert self.headers['Content-Type'] == 'application/x-www-form-urlencoded'
            assert self.headers['Authorization'] == 'Bearer provider-test'
            assert body == {'src': ['magnet:?xt=urn:btih:' + 'a' * 40]}
            self.send_json(200, {'status': 'success'})
        elif path == '/provider/redirect':
            self.send_response(307)
            self.send_header('Location', self.server.base + '/provider/credential-leak')
            self.end_headers()
        elif path.endswith('sync_pull_provider_credentials'):
            assert body == {'p_profile_id': 1}
            assert self.headers['Authorization'] in ('Bearer access-first', 'Bearer access-refreshed')
            self.send_json(503 if state.get('services_offline') else 200, [
                {'provider': 'debrid:torbox', 'credential_json': {'api_key': 'torbox-test'}},
                {'provider': 'debrid:premiumize', 'credential_json': '{"api_key":"premiumize-test"}'},
                {'provider': 'mdblist', 'credential_json': {'api_key': 'unrelated-secret'}}])
        elif path.endswith('sync_pull_profile_settings_blob'):
            assert body == {'p_profile_id': 1, 'p_platform': 'tv'}
            self.send_json(404 if state.get('services_settings_missing') else 200, [{'settings_json': {'features': {
                'debrid_settings': {'debrid_enabled': True, 'preferred_resolver_provider_id': 'premiumize', 'instant_playback_preparation_limit': 3, 'irrelevant': 'ignore'}}}}])
        elif path.startswith('/rest/v1/addons?') or path.startswith('/rest/v1/tv_addons?'):
            if state.get('legacy_addons') and path.startswith('/rest/v1/addons?'):
                self.send_json(404, {'code': 'PGRST205'}); return
            if state.get('addon_transient'):
                self.send_json(503, {}); return
            assert ('user_id=eq.owner-id&profile_id=eq.1' in path or 'owner_id=eq.owner-id' in path)
            endpoint = self.server.base
            self.send_json(200, state.get('addon_rows', [{'url': endpoint + '/metadata/manifest.json', 'enabled': True},
                {'base_url': endpoint + '/streams', 'enabled': 'true'},
                {'url': endpoint + '/disabled/manifest.json', 'enabled': False}]))
        elif pathname == '/streams/ConfigToken/manifest.json':
            assert path.endswith('?apiKey=CaseSensitive&services=RD%2BAD')
            assert self.headers.get('Authorization') is None
            self.send_json(200, {'id': 'configured-aiostreams', 'name': 'Configured AIOStreams',
                'types': ['movie', 'series'], 'resources': ['stream']})
        elif pathname == '/metadata/ConfigToken/manifest.json':
            assert path.endswith('?apiKey=CaseSensitive&language=en-US')
            assert self.headers.get('Authorization') is None
            self.send_json(200, {'id': 'configured-aiometadata', 'name': 'AIOMetadata',
                'types': ['movie', 'series'], 'resources': ['catalog', 'meta'], 'catalogs': [
                    {'type': 'series', 'id': 'tmdb.popular', 'name': 'Popular TV', 'showInHome': True,
                     'extra': [{'name': 'genre', 'isRequired': True, 'options': ['Drama'], 'default': 'Drama'},
                               {'name': 'skip'}]},
                    {'type': 'movie', 'id': 'search', 'extra': [{'name': 'search', 'isRequired': True}]},
                    {'type': 'movie', 'id': 'hidden', 'showInHome': False}]})
        elif pathname == '/metadata/ConfigToken/catalog/series/tmdb.popular.json':
            assert path.endswith('?apiKey=CaseSensitive&language=en-US')
            assert self.headers.get('Authorization') is None
            if state.get('catalog_failure'):
                self.send_json(503, {}); return
            self.send_json(200, {'metas': [
                {'id': 'tt0903747', 'type': 'series', 'name': 'A test series',
                 'poster': 'https://example.invalid/poster.jpg'}]})
        elif pathname == '/streams/ConfigToken/stream/movie/tt123.json':
            assert path.endswith('?apiKey=CaseSensitive&services=RD%2BAD')
            assert self.headers.get('Authorization') is None
            self.send_json(200, {'streams': [
                {'name': 'AIOStreams RD+', 'url': 'https://debrid.example.invalid/play?token=CaseSensitive',
                 'infoHash': 'f' * 40, 'behaviorHints': {'proxyHeaders': {'request': {'Authorization': 'Bearer debrid-only'}}}},
                {'name': 'Raw torrent', 'infoHash': 'e' * 40}]})
        elif path in ('/metadata/manifest.json', '/streams/manifest.json'):
            # Addons receive no Nuvio credentials.
            assert self.headers.get('Authorization') is None
            self.send_json(200, {'id': path.split('/')[1], 'name': 'AIO test addon', 'version': '1.0',
                'types': ['movie', 'series'], 'resources': ['catalog', 'meta', 'stream']})
        elif path.endswith('sync_pull_library'):
            assert body == {'p_profile_id': 1, 'p_limit': 500, 'p_offset': 0}
            self.send_json(200, [{'content_id': 'tt123', 'content_type': 'movie', 'name': 'Test title'}])
        elif path.endswith('sync_pull_watch_progress'):
            assert body == {'p_profile_id': 1}
            self.send_json(503 if state.get('progress_offline') else 200, state.get('progress_rows', []))
        elif path.endswith('sync_push_watch_progress'):
            assert body['p_profile_id'] == 1 and body['p_entries']
            assert body['p_origin_client_id'].startswith('nuvio-ps5-')
            if state.get('progress_offline') or state.get('progress_push_failure'):
                self.send_json(503, {}); return
            for row in body['p_entries']:
                assert set(row) == {'content_id', 'content_type', 'video_id', 'season', 'episode',
                                    'position', 'duration', 'last_watched', 'progress_key'}
            state['progress_rows'] = body['p_entries']
            self.send_json(200, {})
        elif path.endswith('sync_delete_watch_progress'):
            assert body['p_profile_id'] == 1 and body['p_origin_client_id'].startswith('nuvio-ps5-')
            if state.get('progress_offline'):
                self.send_json(503, {}); return
            state['progress_rows'] = [r for r in state.get('progress_rows', []) if r['progress_key'] not in body['p_keys']]
            self.send_json(200, {})
        else:
            self.send_json(404, {})

    def send_json(self, status, data):
        body = json.dumps(data).encode()
        self.send_response_only(status)
        if not self.server.state.get('omit_date'):
            date = self.server.state.get('date_header', formatdate(self.server.state.get('server_epoch', time.time()), usegmt=True))
            self.send_header('Date', date)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class AccountTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)
        cert, key = self.path / 'cert.pem', self.path / 'key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
            '-keyout', str(key), '-out', str(cert), '-days', '1', '-subj', '/CN=127.0.0.1',
            '-addext', 'subjectAltName=IP:127.0.0.1'], check=True, capture_output=True)
        self.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Backend)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        self.server.socket = context.wrap_socket(self.server.socket, server_side=True)
        self.server.base = f'https://127.0.0.1:{self.server.server_port}'
        self.state = self.server.state = {'calls': []}
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.config = self.path / 'config.json'
        self.config.write_text(json.dumps({'env': {'NUVIO_SUPABASE_URL': self.server.base,
            'NUVIO_SUPABASE_ANON_KEY': 'public-test-key'}}))
        self.launch(cert)

    def launch(self, ca):
        self.proc = subprocess.Popen([str(BINARY), str(self.config), str(self.path), str(ca)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def command(self, op, **kwargs):
        self.proc.stdin.write(json.dumps(dict(op=op, **kwargs)) + '\n'); self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            self.assertTrue(line, 'host driver exited unexpectedly')
            if line.startswith('{'):
                return json.loads(line)

    def pair(self):
        start = self.command('start')
        self.assertTrue(start['ok'], start)
        self.assertEqual(start['display'], 'ABCD-EF')
        self.assertEqual(start['interval'], 5)
        self.state['approved'] = True
        result = self.command('poll', code=start['code'])
        return result

    def close(self):
        if self.proc.poll() is None:
            self.proc.stdin.close(); self.proc.wait(timeout=5)
        self.proc.stdout.close()

    def tearDown(self):
        self.close()
        self.server.shutdown(); self.server.server_close(); self.thread.join()
        self.temp.cleanup()

    def test_pair_pending_then_save_and_restore_without_pairing(self):
        start = self.command('start')
        self.assertTrue(start['ok'])
        pending = self.command('poll', code=start['code'])
        self.assertFalse(pending['ok']); self.assertEqual(pending['error'], '')
        self.state['approved'] = True
        self.assertTrue(self.command('poll', code=start['code'])['ok'])
        self.assertEqual((self.path / 'account.json').stat().st_mode & 0o777, 0o600)
        self.assertNotEqual(json.loads((self.path / 'account.json').read_text())['access_token'], 'anonymous')
        before = len(self.state['calls'])
        self.close(); self.launch(self.path / 'cert.pem')
        self.assertTrue(self.command('saved')['saved'])
        self.assertTrue(self.command('user')['ok'])
        self.assertEqual([r[0] for r in self.state['calls'][before:]], ['/auth/v1/user'])

    def test_update_metadata_rejects_plaintext_http(self):
        before = len(self.state['calls'])
        result = self.command('https_update', url=self.server.base.replace('https:', 'http:') + '/update/feed')
        self.assertFalse(result['ok'])
        self.assertTrue(result['error'])
        self.assertEqual(len(self.state['calls']), before)

    def test_provider_credentials_require_signed_in_nuvio(self):
        before = len(self.state['calls'])
        self.assertFalse(self.command('connected_services')['ok'])
        self.assertEqual(len(self.state['calls']), before)

    def test_provider_credential_import_is_read_only_and_scoped(self):
        self.assertTrue(self.pair()['ok'])
        result = self.command('connected_services')
        self.assertTrue(result['ok'], result)
        self.assertEqual(result['result']['owner_id'], 'owner-id')
        self.assertEqual(result['result']['account_user_id'], 'owner-id')
        self.assertEqual(result['result']['preferred'], 'premiumize')
        self.assertTrue(result['result']['enabled'])
        self.assertEqual(result['result']['instant_limit'], 3)
        self.assertEqual([r['provider'] for r in result['result']['credentials']], ['debrid:torbox', 'debrid:premiumize'])
        self.assertNotIn('unrelated-secret', str(result))
        self.assertFalse(any('push_provider' in r[0] or 'seed_provider' in r[0] for r in self.state['calls']))

    def test_provider_import_survives_missing_settings_rpc(self):
        self.assertTrue(self.pair()['ok'])
        self.state['services_settings_missing'] = True
        result = self.command('connected_services')
        self.assertTrue(result['ok'], result)
        self.assertEqual(len(result['result']['credentials']), 2)
        self.assertNotIn('preferred', result['result'])

    def test_provider_import_reports_offline_without_credentials(self):
        self.assertTrue(self.pair()['ok'])
        self.state['services_offline'] = True
        result = self.command('connected_services')
        self.assertFalse(result['ok'])
        self.assertNotIn('torbox-test', str(result))

    def test_provider_transport_posts_urlencoded_form(self):
        result = self.command('provider_request', url=self.server.base + '/provider/form', post=True,
            body='src=magnet%3A%3Fxt%3Durn%3Abtih%3A' + 'a' * 40)
        self.assertTrue(result['ok'], result)

    def test_provider_transport_rejects_http_and_never_forwards_credentials_on_redirect(self):
        before = len(self.state['calls'])
        result = self.command('provider_request', url=self.server.base.replace('https:', 'http:') + '/provider/form')
        self.assertFalse(result['ok'])
        self.assertEqual(len(self.state['calls']), before)
        result = self.command('provider_request', url=self.server.base + '/provider/redirect')
        self.assertFalse(result['ok'])
        self.assertEqual(result['status'], 307)
        self.assertEqual(self.state['calls'][-1][0], '/provider/redirect')
        self.assertFalse(any(r[0] == '/provider/credential-leak' for r in self.state['calls']))

    def test_update_metadata_rejects_tls_to_plaintext_redirect(self):
        result = self.command('https_update', url=self.server.base + '/update/redirect')
        self.assertFalse(result['ok'])
        self.assertTrue(result['error'])
        self.assertEqual(self.state['calls'][-1][0], '/update/redirect')
        self.assertIsNone(self.state['calls'][-1][2])

    def test_home_catalog_selection_matches_nuvio(self):
        catalogs = [
            {'id': 'popular', 'type': 'series', 'showInHome': True,
             'extra': [{'name': 'genre', 'isRequired': True}, {'name': 'skip'}]},
            {'id': 'required-skip', 'type': 'movie', 'extra': [{'name': 'skip', 'isRequired': True}]},
            {'id': 'hidden', 'type': 'movie', 'showInHome': False},
            {'id': 'search', 'type': 'movie', 'showInHome': True,
             'extra': [{'name': 'search', 'isRequired': True}]},
            {'id': 'legacy-search', 'type': 'series', 'extraRequired': ['search']},
            {'id': 'normalized-search', 'type': 'series',
             'extra': [{'name': ' SEARCH ', 'isRequired': 'true'}]},
            {'id': 'null-home-setting', 'type': 'series', 'showInHome': None},
            {'id': 'ordinary', 'type': 'series', 'extra': [{'name': 'genre'}]}]
        result = self.command('catalog_manifest', manifest={'id': 'metadata', 'catalogs': catalogs},
                              url='https://example.invalid/ConfigToken?language=en-US')
        self.assertTrue(result['ok'])
        self.assertEqual([c['id'] for c in result['catalogs'] if c['home']],
                         ['popular', 'required-skip', 'null-home-setting', 'ordinary'])
        self.assertEqual(result['catalogs'][0]['url'],
                         'https://example.invalid/ConfigToken/catalog/series/popular.json?language=en-US')

    def test_synced_aiometadata_popular_catalog_is_requested(self):
        self.assertTrue(self.pair()['ok'])
        self.state['addon_rows'] = [{'url': self.server.base +
            '/metadata/ConfigToken?apiKey=CaseSensitive&language=en-US', 'enabled': True}]
        result = self.command('account_catalogs')
        self.assertTrue(result['ok'], result)
        self.assertEqual(len(result['rows']), 1, result)
        self.assertTrue(result['rows'][0]['ok'], result)
        self.assertEqual(result['rows'][0]['metas'][0]['name'], 'A test series')
        calls = [path for path, _, _ in self.state['calls'] if '/catalog/' in path]
        self.assertEqual(calls, ['/metadata/ConfigToken/catalog/series/tmdb.popular.json?apiKey=CaseSensitive&language=en-US'])

    def test_catalog_request_failure_is_reported(self):
        self.assertTrue(self.pair()['ok'])
        self.state.update(addon_rows=[{'url': self.server.base +
            '/metadata/ConfigToken?apiKey=CaseSensitive&language=en-US', 'enabled': True}], catalog_failure=True)
        result = self.command('account_catalogs')
        self.assertTrue(result['ok'], result)
        self.assertFalse(result['rows'][0]['ok'])
        self.assertIn('503', result['rows'][0]['error'])

    def test_refresh_rotation_serializes_parallel_requests(self):
        self.state['expired'] = True
        self.assertTrue(self.pair()['ok'])
        self.assertEqual(self.command('parallel')['count'], 8)
        calls = [r for r in self.state['calls'] if 'grant_type=refresh_token' in r[0]]
        self.assertEqual(len(calls), 1)
        self.assertEqual(json.loads((self.path / 'account.json').read_text())['refresh_token'], 'refresh-rotated')

    def test_pairing_expiry_uses_server_time_when_console_clock_is_ahead(self):
        # The backend is ten minutes away from expiry on its own clock. The
        # client wall clock is years ahead: this previously collapsed TTL to 1s.
        self.state.update(server_epoch=946684800, pairing_fields={
            'expires_in': None, 'expires_at': '2000-01-01T00:10:00.000Z'})
        start = self.command('start')
        self.assertTrue(start['ok'], start)
        self.assertEqual(start['expires_in'], 600, start)

    def test_relative_pairing_lifetime_takes_priority(self):
        self.state.update(server_epoch=946684800, pairing_fields={
            'expires_in': 300, 'expires_at': '1999-12-31T23:59:00Z'})
        start = self.command('start')
        self.assertTrue(start['ok'], start)
        self.assertEqual(start['expires_in'], 300)

    def test_absolute_pairing_timezones_and_fractional_seconds(self):
        for timestamp, epoch, lifetime in [
            ('2000-01-01T02:10:00+02:00', 946684800, 600),
            ('1999-12-31T19:10:00-05:00', 946684800, 600),
            ('2000-01-01T00:09:59.500Z', 946684800, 600),
            ('2000-02-29T00:10:00.000+00:00', 951782400, 600),
        ]:
            with self.subTest(timestamp=timestamp):
                self.state.update(server_epoch=epoch, pairing_fields={
                    'expires_in': None, 'expires_at': timestamp})
                start = self.command('start')
                self.assertTrue(start['ok'], start)
                self.assertEqual(start['expires_in'], lifetime)

    def test_missing_or_invalid_server_date_keeps_code_pending(self):
        self.state['pairing_fields'] = {'expires_in': None, 'expires_at': '2000-01-01T00:10:00Z'}
        for date in (None, 'invalid-date'):
            with self.subTest(date=date):
                self.state.update(omit_date=date is None, date_header=date)
                start = self.command('start')
                self.assertTrue(start['ok'], start)
                self.assertEqual(start['expires_in'], 600)
                pending = self.command('poll', code=start['code'])
                self.assertFalse(pending['ok'])
                self.assertFalse(pending['result']['terminal'])
        self.state['approved'] = True
        self.assertTrue(self.command('poll', code=start['code'])['ok'])

    def test_unusable_absolute_expiry_uses_bounded_fallback(self):
        for timestamp in (None, 'bad', '2000-02-30T00:10:00Z', '2000-01-01T00:10:00.'):
            with self.subTest(timestamp=timestamp):
                self.state.update(server_epoch=946684800, pairing_fields={
                    'expires_in': None, 'expires_at': timestamp})
                start = self.command('start')
                self.assertTrue(start['ok'], start)
                self.assertEqual(start['expires_in'], 600)

    def test_numeric_absolute_pairing_expiry(self):
        for field, value in [('expires_at', 946685400), ('expires_at', 946685400000),
                             ('expires_at_millis', 946685400000)]:
            with self.subTest(field=field, value=value):
                self.state.update(server_epoch=946684800, pairing_fields={
                    'expires_in': None, 'expires_at': None, 'expires_at_millis': None, field: value})
                start = self.command('start')
                self.assertTrue(start['ok'], start)
                self.assertEqual(start['expires_in'], 600)

    def test_server_expired_code_is_rejected(self):
        for fields in ({'expires_in': 0},
                       {'expires_in': None, 'expires_at': '2000-01-01T00:00:00Z'}):
            with self.subTest(fields=fields):
                self.state.update(server_epoch=946684800, pairing_fields=fields)
                start = self.command('start')
                self.assertFalse(start['ok'])
                self.assertIn('expired', start['error'])
                self.assertFalse(self.command('saved')['saved'])

    def test_backend_expiry_and_rejection_end_pairing(self):
        start = self.command('start')
        for status in ('expired', 'denied', 'rejected', 'cancelled', 'consumed'):
            with self.subTest(status=status):
                self.state['pairing_status'] = status
                result = self.command('poll', code=start['code'])
                self.assertFalse(result['ok'])
                self.assertTrue(result['result']['terminal'])
        self.assertFalse(any(r[0].endswith('tv-logins-exchange') for r in self.state['calls']))

    def test_server_time_prevents_refresh_loop_after_pairing_and_restore(self):
        self.state['server_epoch'] = 946684800
        self.assertTrue(self.pair()['ok'])
        self.assertEqual(self.command('parallel')['count'], 8)
        self.assertFalse(any('grant_type=refresh_token' in r[0] for r in self.state['calls']))
        self.close(); self.launch(self.path / 'cert.pem')
        self.assertEqual(self.command('parallel')['count'], 8)
        refreshes = [r for r in self.state['calls'] if 'grant_type=refresh_token' in r[0]]
        self.assertEqual(len(refreshes), 1)
        self.assertEqual(json.loads((self.path / 'account.json').read_text())['refresh_token'], 'refresh-rotated')

    def test_missing_rotated_refresh_token_retains_previous(self):
        self.state.update(expired=True, omit_refresh=True)
        self.assertTrue(self.pair()['ok']); self.assertTrue(self.command('user')['ok'])
        self.assertEqual(json.loads((self.path / 'account.json').read_text())['refresh_token'], 'refresh-first')

    def test_transient_refresh_error_preserves_session(self):
        self.state.update(expired=True, transient=True)
        self.assertTrue(self.pair()['ok'])
        before = (self.path / 'account.json').read_bytes()
        self.assertFalse(self.command('user')['ok'])
        self.assertTrue(self.command('saved')['saved'])
        self.assertEqual(before, (self.path / 'account.json').read_bytes())
        self.state['transient'] = False
        self.assertTrue(self.command('user')['ok'])

    def test_revoked_refresh_token_clears_session(self):
        self.state.update(expired=True, revoked=True)
        self.assertTrue(self.pair()['ok']); self.assertFalse(self.command('user')['ok'])
        self.assertFalse(self.command('saved')['saved'])
        self.assertEqual(json.loads((self.path / 'account.json').read_text()), {})

    def test_anonymous_exchange_is_not_saved_as_login(self):
        self.state['anonymous_exchange'] = True
        self.assertFalse(self.pair()['ok'])
        self.assertFalse(self.command('saved')['saved'])
        self.assertFalse((self.path / 'account.json').exists())

    def test_cancelled_pairing_cannot_exchange_or_persist(self):
        start = self.command('start'); self.state['approved'] = True
        self.command('cancel')
        self.assertFalse(self.command('poll', code=start['code'])['ok'])
        self.assertFalse((self.path / 'account.json').exists())
        self.assertFalse(any(r[0].endswith('tv-logins-exchange') for r in self.state['calls']))

    def test_legacy_pairing_and_addon_fallback(self):
        self.state.update(legacy=True, legacy_addons=True)
        self.assertTrue(self.pair()['ok'])
        addons = self.command('addons'); self.assertTrue(addons['ok'], addons)
        self.assertEqual(len(addons['result']['addons']), 2)
        self.assertFalse(any('disabled/manifest' in r[0] for r in self.state['calls']))
        self.assertTrue(self.command('library')['ok'])

    def test_storage_failure_does_not_report_saved_login(self):
        (self.path / 'account.json').mkdir()
        self.assertFalse(self.pair()['ok']); self.assertFalse(self.command('saved')['saved'])

    def test_transient_addon_sync_uses_cache_after_restart(self):
        self.assertTrue(self.pair()['ok'])
        before = self.command('addons'); self.assertTrue(before['ok'])
        self.state['addon_transient'] = True
        self.close(); self.launch(self.path / 'cert.pem')
        after = self.command('addons'); self.assertTrue(after['ok'])
        self.assertTrue(after['result']['offline'])
        self.assertEqual(before['result']['addons'], after['result']['addons'])

    def test_logout_survives_process_restart(self):
        self.assertTrue(self.pair()['ok']); self.assertTrue(self.command('logout')['ok'])
        self.close(); self.launch(self.path / 'cert.pem')
        self.assertFalse(self.command('saved')['saved'])

    def test_untrusted_https_backend_is_rejected(self):
        self.close(); self.launch('/etc/ssl/certs/ca-certificates.crt')
        self.assertFalse(self.command('start')['ok'])
        self.assertFalse(self.state['calls'])

    def test_aiostreams_http_headers_and_torrent_entries(self):
        resource = {'streams': [{'url': 'https://example.invalid/video.mkv',
            'behaviorHints': {'proxyHeaders': {'request': {'Authorization': 'Bearer stream-only', 'Referer': 'https://example.invalid'}}}},
            {'infoHash': 'abcd', 'fileIdx': 1}]}
        result = self.command('streams', resource=resource)
        self.assertEqual(result['count'], 2)
        self.assertIn('Authorization: Bearer stream-only', result['headers'])

    def test_configured_addon_url_preserves_path_and_query_on_resources(self):
        query = '?apiKey=CaseSensitive&services=RD%2BAD'
        for url in ('https://addon.example/ConfigToken' + query,
                    'https://addon.example/ConfigToken/manifest.json' + query,
                    'stremio://addon.example/ConfigToken/' + query):
            with self.subTest(url=url):
                result = self.command('addon_url', url=url)
                self.assertTrue(result['ok'])
                self.assertEqual(result['manifest'], 'https://addon.example/ConfigToken/manifest.json' + query)
                self.assertEqual(result['resource'], 'https://addon.example/ConfigToken/stream/movie/tt123.json' + query)

    def test_synced_configured_addon_returns_debrid_links(self):
        self.state['addon_rows'] = [{'baseUrl': self.server.base +
            '/streams/ConfigToken?apiKey=CaseSensitive&services=RD%2BAD', 'enabled': True}]
        self.assertTrue(self.pair()['ok'])
        result = self.command('account_streams')
        self.assertTrue(result['ok'], result)
        self.assertEqual(result['count'], 2)
        self.assertEqual(result['allowed_count'], 1)
        stream = result['streams'][0]
        self.assertEqual(stream['url'], 'https://debrid.example.invalid/play?token=CaseSensitive')
        self.assertFalse(stream['torrent'])
        self.assertTrue(stream['direct'] and stream['allowed'])
        self.assertIn('Authorization: Bearer debrid-only', stream['headers'])

    def test_debrid_url_wins_over_accompanying_torrent_hash(self):
        result = self.command('streams', resource={'streams': [{
            'url': 'https://debrid.example.invalid/video.mkv', 'infoHash': 'f' * 40,
            'behaviorHints': {'notWebReady': True, 'proxyHeaders': {'request': {'Referer': 'https://addon.example'}}}}]})
        self.assertEqual(result['allowed_count'], 1)
        self.assertFalse(result['streams'][0]['torrent'])
        self.assertIn('Referer: https://addon.example', result['headers'])

    def test_raw_torrents_require_opt_in(self):
        resource = {'streams': [{'infoHash': 'f' * 40}]}
        default = self.command('streams', resource=resource)
        self.assertFalse(default['torrents_default'])
        self.assertEqual(default['allowed_count'], 0)
        enabled = self.command('streams', resource=resource, torrents=True)
        self.assertEqual(enabled['allowed_count'], 1)

    def progress_item(self, content='tt123', video=None, position=120000, duration=7200000, **state):
        return {'_id': content, 'type': 'series' if ':' in (video or '') else 'movie', 'name': 'Test title',
                'poster': 'https://example.invalid/poster',
                'state': dict(timeOffset=position, duration=duration, video_id=video or content, **state)}

    def progress_row(self, content='tt123', position=240000, duration=7200000, **extra):
        return dict(content_id=content, content_type='movie', video_id=content, position=position,
                    duration=duration, last_watched=int(time.time() * 1000), progress_key=content, **extra)

    def test_watch_progress_loads_remote_resume_and_legacy_seconds(self):
        self.assertTrue(self.pair()['ok'])
        self.state['progress_rows'] = [self.progress_row(position=120, duration=7200)]
        result = self.command('library')['result'][0]
        self.assertEqual(result['state']['timeOffset'], 120000)
        self.assertEqual(result['state']['duration'], 7200000)
        self.assertEqual(result['name'], 'Test title')

    def test_watch_progress_explicit_units_and_profile_isolation(self):
        self.assertTrue(self.pair()['ok'])
        self.state['progress_rows'] = [self.progress_row(position_ms=3000, duration_ms=7200000),
                                      self.progress_row('other', profile_id=2)]
        result = self.command('library')['result']
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['state']['timeOffset'], 3000)

    def test_watch_progress_pushes_only_portable_fields(self):
        self.assertTrue(self.pair()['ok'])
        item = self.progress_item()
        item['state']['stream'] = 'https://private.invalid/play?token=secret'
        result = self.command('progress_write', changes=[item])
        self.assertTrue(result['ok']); self.assertTrue(result['result']['progressSynced'])
        row = self.state['progress_rows'][0]
        self.assertEqual(row['position'], 120000); self.assertEqual(row['duration'], 7200000)
        self.assertNotIn('private', json.dumps(row)); self.assertNotIn('poster', row)
        self.assertEqual((self.path / 'watch-sync.json').stat().st_mode & 0o777, 0o600)

    def test_watch_progress_push_preserves_other_device_rows_and_utc_offsets(self):
        self.assertTrue(self.pair()['ok'])
        self.state['progress_rows'] = [self.progress_row('tt999', updated_at='2026-10-08T15:30:00.123+02:30')]
        self.command('progress_write', changes=[self.progress_item()])
        self.assertEqual({r['content_id'] for r in self.state['progress_rows']}, {'tt123', 'tt999'})
        imported = next(r for r in self.state['progress_rows'] if r['content_id'] == 'tt999')
        self.assertEqual(imported['last_watched'], 1791464400123)

    def test_watch_progress_keeps_episode_keys_separate(self):
        self.assertTrue(self.pair()['ok'])
        for episode in (1, 2):
            item = self.progress_item(video=f'tt123:1:{episode}', nuvioSeason=1, nuvioEpisode=episode)
            self.assertTrue(self.command('progress_write', changes=[item])['ok'])
        self.assertEqual({r['progress_key'] for r in self.state['progress_rows']}, {'tt123_s1e1', 'tt123_s1e2'})
        self.assertEqual(len(json.loads((self.path / 'watch-sync.json').read_text())['entries']), 2)

    def test_watch_progress_completion_is_portable(self):
        self.assertTrue(self.pair()['ok'])
        item = self.progress_item(position=0, nuvioCompleted=True)
        self.assertTrue(self.command('progress_write', changes=[item])['ok'])
        self.assertEqual(self.state['progress_rows'][0]['position'], 7200000)
        state = self.command('library')['result'][0]['state']
        self.assertEqual(state['timeOffset'], 0); self.assertTrue(state['nuvioCompleted'])

    def test_watch_progress_failed_push_retries_after_restart(self):
        self.assertTrue(self.pair()['ok']); self.state['progress_push_failure'] = True
        result = self.command('progress_write', changes=[self.progress_item()])
        self.assertTrue(result['ok']); self.assertFalse(result['result']['progressSynced'])
        self.assertEqual(len(json.loads((self.path / 'watch-sync.json').read_text())['pending']), 1)
        self.close(); self.launch(self.path / 'cert.pem'); self.state['progress_push_failure'] = False
        self.assertTrue(self.command('library')['ok'])
        self.assertEqual(self.state['progress_rows'][0]['position'], 120000)
        self.assertEqual(json.loads((self.path / 'watch-sync.json').read_text())['pending'], [])

    def test_watch_progress_newer_remote_wins_over_offline_update(self):
        self.assertTrue(self.pair()['ok']); self.state['progress_offline'] = True
        self.assertTrue(self.command('progress_write', changes=[self.progress_item()])['ok'])
        self.close(); self.launch(self.path / 'cert.pem'); self.state['progress_offline'] = False
        row = self.progress_row(position=600000); row['last_watched'] += 60000
        self.state['progress_rows'] = [row]
        result = self.command('library')['result'][0]
        self.assertEqual(result['state']['timeOffset'], 600000)
        self.assertFalse(any(p.endswith('sync_push_watch_progress') for p, _, _ in self.state['calls']))

    def test_watch_progress_remove_uses_delete_and_stable_client_id(self):
        self.assertTrue(self.pair()['ok'])
        self.command('progress_write', changes=[self.progress_item()])
        client = json.loads((self.path / 'watch-sync.json').read_text())['client']
        self.close(); self.launch(self.path / 'cert.pem')
        item = self.progress_item(position=0, nuvioRemoveProgress=True)
        self.assertTrue(self.command('progress_write', changes=[item])['ok'])
        calls = [body for path, body, _ in self.state['calls'] if path.endswith('sync_delete_watch_progress')]
        self.assertEqual(calls[0]['p_keys'], ['tt123']); self.assertEqual(calls[0]['p_origin_client_id'], client)
        self.assertEqual(self.state['progress_rows'], [])
        self.assertEqual(self.command('library')['result'][0]['state']['timeOffset'], 0)

    def test_watch_progress_empty_or_invalid_snapshot_keeps_saved_resume(self):
        self.assertTrue(self.pair()['ok'])
        self.command('progress_write', changes=[self.progress_item()])
        self.state['progress_rows'] = []
        self.assertEqual(self.command('library')['result'][0]['state']['timeOffset'], 120000)
        self.state['progress_rows'] = {'unexpected': 'object'}
        self.assertEqual(self.command('library')['result'][0]['state']['timeOffset'], 120000)
        pushes = [body for path, body, _ in self.state['calls'] if path.endswith('sync_push_watch_progress')]
        self.assertEqual(len(pushes), 1); self.assertTrue(pushes[0]['p_entries'])

    def test_watch_progress_remote_deletion_does_not_restore_old_local_state(self):
        self.assertTrue(self.pair()['ok'])
        self.command('progress_write', changes=[self.progress_item()])
        self.state['progress_rows'] = [self.progress_row('tt999')]
        items = self.command('library')['result']
        old = next(i for i in items if i['_id'] == 'tt123')
        new = next(i for i in items if i['_id'] == 'tt999')
        self.assertEqual(old['state']['timeOffset'], 0)
        self.assertEqual(new['state']['timeOffset'], 240000); self.assertTrue(new['temp'])

    def test_watch_progress_logout_clears_account_cache(self):
        self.assertTrue(self.pair()['ok']); self.command('progress_write', changes=[self.progress_item()])
        self.assertTrue((self.path / 'watch-sync.json').exists())
        self.assertTrue(self.command('logout')['ok'])
        self.assertFalse((self.path / 'watch-sync.json').exists())

    def test_watch_progress_delayed_write_cannot_cross_login_boundary(self):
        key = self.pair()['result']['authKey']
        self.assertTrue(self.command('logout')['ok'])
        self.assertTrue(self.pair()['ok'])
        result = self.command('progress_write', auth_key=key, changes=[self.progress_item()])
        self.assertFalse(result['ok'])
        self.assertFalse((self.path / 'native-library.json').exists())
        self.assertFalse(any(path.endswith('sync_push_watch_progress') for path, _, _ in self.state['calls']))

    def test_watch_progress_short_clip_does_not_upload(self):
        self.assertTrue(self.pair()['ok'])
        self.command('progress_write', changes=[self.progress_item(duration=10000)])
        self.assertFalse(any(path.endswith('sync_push_watch_progress') for path, _, _ in self.state['calls']))

    def test_unsupported_url_does_not_fall_back_to_torrent(self):
        resource = {'streams': [{'url': 'ftp://example.invalid/video', 'infoHash': 'f' * 40},
            {'externalUrl': 'https://external.example.invalid'},
            {'url': 'magnet:?xt=urn:btih:' + 'e' * 40}]}
        result = self.command('streams', resource=resource)
        self.assertEqual(result['count'], 1)
        self.assertEqual(result['allowed_count'], 0)
        self.assertTrue(result['streams'][0]['torrent'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
