import json,os,pathlib,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ReaderTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name);(self.root/'usb0/nested').mkdir(parents=True)
        self.body=b'#EXTM3U\n#EXTINF:-1,News\nhttps://example.test/live.m3u8\n';(self.root/'usb0/nested/TV.M3U8').write_bytes(self.body)
        (self.root/'usb0/link.m3u').symlink_to(self.root/'usb0/nested/TV.M3U8');(self.root/'usb0/linked').symlink_to(self.root/'usb0/nested',target_is_directory=True)
        self.env=dict(os.environ,NUVIO_USB_READER_TEST_ROOT=str(self.root))
    def tearDown(self):self.temp.cleanup()
    def start(self,request):
        p=subprocess.Popen([str(ROOT/'test-output/usb-reader-helper')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=self.env)
        p.stdin.write(json.dumps(request).encode()+b'\n');p.stdin.flush();return p
    def finish(self,p):
        try:return p.wait(timeout=5)
        finally:
            if p.poll() is None:p.kill();p.wait()
            p.stdin.close();p.stdout.close();p.stderr.close()
    def request(self,op,**fields):return dict(protocol=1,title_id='PPSA99288',op=op,**fields)
    def test_scan_and_read_share_bounded_protocol(self):
        p=self.start(self.request('scan'));lines=[]
        while True:
            row=json.loads(p.stdout.readline());lines.append(row)
            if row['kind'] in ('done','error'):break
        self.assertEqual(self.finish(p),0);self.assertEqual(lines[0]['kind'],'ready')
        mounts=[r for r in lines if r['kind']=='mount'];self.assertEqual(len(mounts),8);self.assertEqual(mounts[0]['directories'],2)
        files=[r for r in lines if r['kind']=='file'];self.assertEqual([(r['mount'],r['path']) for r in files],[(0,'nested/TV.M3U8')]);self.assertEqual(lines[-1]['files'],1)
        p=self.start(self.request('read',mount=0,path='nested/TV.M3U8'));self.assertEqual(json.loads(p.stdout.readline())['kind'],'ready');row=json.loads(p.stdout.readline());self.assertEqual(row['bytes'],len(self.body));self.assertEqual(p.stdout.read(len(self.body)),self.body);self.assertEqual(self.finish(p),0)
    def test_rejects_foreign_titles_traversal_mounts_and_commands(self):
        cases=[self.request('write'),dict(self.request('scan'),title_id='PPSA01650'),dict(self.request('scan'),extra='ignored?'),self.request('read',mount=8,path='x.m3u'),self.request('read',mount=-1,path='x.m3u')]
        for path in ['../profile/x.m3u','/x.m3u','a//x.m3u','a/./x.m3u','a/../x.m3u','a\\x.m3u','a\x00.m3u','a/b/c/d/e/x.m3u','x.txt','x.m3u/']:
            cases.append(self.request('read',mount=0,path=path))
        for request in cases:
            with self.subTest(request=request):
                p=self.start(request);row=json.loads(p.stdout.readline());self.assertEqual(row,dict(kind='error',code='invalid-request'));self.assertNotEqual(self.finish(p),0)
    def test_symlinks_missing_and_oversized_files_rejected_without_details(self):
        with open(self.root/'usb0/huge.m3u','wb') as f:f.truncate((128<<20)+1)
        for path in ['link.m3u','linked/TV.M3U8','missing.m3u','huge.m3u']:
            p=self.start(self.request('read',mount=0,path=path));self.assertEqual(json.loads(p.stdout.readline())['kind'],'ready');row=json.loads(p.stdout.readline());self.assertEqual(row,dict(kind='error',code='read-rejected'));self.assertNotEqual(self.finish(p),0)
    def test_peer_disconnect_terminates_reader(self):
        p=self.start(self.request('read',mount=0,path='nested/TV.M3U8'));p.stdin.close();p.stdout.close()
        try:p.wait(timeout=5)
        finally:
            if p.poll() is None:p.kill();p.wait()
            p.stderr.close()
if __name__=='__main__':unittest.main()
