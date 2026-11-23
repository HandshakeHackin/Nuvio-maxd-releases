"""Regression checks for the exact build-time catalog compiler."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('build_locales', root / 'tools/build_locales.py')
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)

class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name) / 'locales'
        shutil.copytree(root / 'ui/locales', self.directory)
        self.addCleanup(self.temp.cleanup)
    def mutate(self, name, fn):
        p = self.directory / name
        data = json.loads(p.read_text())
        fn(data)
        p.write_text(json.dumps(data, ensure_ascii=False))
    def reject(self):
        with self.assertRaises((ValueError, TypeError)):
            compiler.compile_catalogs(self.directory)
    def test_reproducible_and_complete_core(self):
        output, stats = compiler.compile_catalogs(self.directory)
        self.assertEqual(output, (self.directory / 'catalogs.inc').read_text())
        self.assertEqual(len(stats), 8)
        self.assertTrue(all(s['coreComplete'] for s in stats.values()))
        self.assertGreaterEqual(stats['ar']['core'], 350)
    def test_missing_core_is_rejected(self):
        self.mutate('ar.json', lambda d: d['strings'].pop('Settings'))
        self.reject()
    def test_advanced_help_can_fall_back(self):
        self.mutate('de.json', lambda d: d['strings'].pop('Choose a colour that is comfortable to read.'))
        _, stats = compiler.compile_catalogs(self.directory)
        self.assertLess(stats['de']['translated'], stats['en']['translated'])
    def test_unknown_key_is_rejected(self):
        self.mutate('fr.json', lambda d: d['strings'].update({'not-a-canonical-key': 'x'}))
        self.reject()
    def test_size_t_printf_argument_is_preserved(self):
        self.mutate('ko.json', lambda d: d['strings'].update({'%zu streams': '%s'}))
        self.reject()
    def test_printf_width_is_preserved(self):
        self.mutate('ja.json', lambda d: d['strings'].update({'Resume from %d:%02d': '%d:%d'}))
        self.reject()
    def test_format_write_conversion_is_rejected(self):
        self.mutate('es.json', lambda d: d['strings'].update({'Settings': '%n'}))
        # The dangerous conversion is rejected even on a key without formats.
        self.reject()
    def test_empty_null_and_hidden_bidi_are_rejected(self):
        for value in ['', None, 'x\0y', '\u202etext']:
            original=(root/'ui/locales/ar.json').read_bytes()
            (self.directory/'ar.json').write_bytes(original)
            self.mutate('ar.json', lambda d: d['strings'].update({'Settings': value}))
            self.reject()
    def test_duplicate_json_key_is_rejected(self):
        p=self.directory/'es.json'
        p.write_text(p.read_text().replace('"strings": {','"strings": {"Settings":"x",',1))
        self.reject()
    def test_language_path_is_rejected(self):
        self.mutate('languages.json', lambda d: d[1].update(code='../../de'))
        self.reject()
    def test_language_identity_is_checked(self):
        self.mutate('fr.json', lambda d: d.update(targetLanguage='ja'))
        self.reject()
    def test_stale_generated_output_is_rejected(self):
        (self.directory/'catalogs.inc').write_text('// stale')
        result=subprocess.run([sys.executable,str(root/'tools/build_locales.py'),'--check','--directory',str(self.directory)],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('translation table is stale', result.stderr)

if __name__ == '__main__':
    unittest.main()
