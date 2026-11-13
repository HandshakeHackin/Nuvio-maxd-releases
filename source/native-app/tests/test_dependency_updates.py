import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dependency_updates', ROOT / 'tools/check_dependency_updates.py')
updates = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updates)


class DependencyUpdateTests(unittest.TestCase):
    def evaluate(self, tags, current='1.6.59', line=None):
        return updates.evaluate('fixture', 'example/repo', current, line, [{'name': tag} for tag in tags])

    def test_unrelated_pngmeta_tags_do_not_report_libpng_upgrade(self):
        result = self.evaluate(['v1.6.59', 'pngmeta_1.11', 'pngmeta_1.10', 'v1.7.0beta89'])
        self.assertEqual(result['selectedTag'], 'v1.6.59')
        self.assertFalse(result['updateAvailable'])

    def test_prereleases_and_development_tags_are_not_stable(self):
        result = self.evaluate(['v1.6.59', 'v2.0.0-rc1', 'v2.0.0beta1', 'n9.1-dev'])
        self.assertFalse(result['updateAvailable'])

    def test_numeric_versions_are_compared_across_all_pages(self):
        raw = '[{"name":"v1.6.9"}]\n[{"name":"v1.6.60"},{"name":"v1.6.59"}]'
        result = updates.evaluate('fixture', 'example/repo', '1.6.59', None, updates.decode_pages(raw))
        self.assertTrue(result['updateAvailable'])
        self.assertEqual(result['selectedTag'], 'v1.6.60')

    def test_new_major_release_is_visible_without_replacing_selected_lts(self):
        result = self.evaluate(['openssl-3.5.9', 'openssl-4.1.0'], '3.5.9', (3, 5))
        self.assertFalse(result['updateAvailable'])
        self.assertTrue(result['newerReleaseOutsideSelectedLine'])
        self.assertEqual(result['latestStableTag'], 'openssl-4.1.0')

    def test_new_selected_line_patch_is_reported(self):
        result = self.evaluate(['n7.0.3', 'n7.0.4', 'n9.0.2'], '7.0.3', (7, 0))
        self.assertTrue(result['updateAvailable'])
        self.assertEqual(result['selectedTag'], 'n7.0.4')

    def test_official_tag_formats_and_equivalent_two_part_version(self):
        for tag, expected in [('curl-8_23_0', (8, 23, 0)), ('R_2_10_0', (2, 10, 0)), ('VER-2-15-0', (2, 15, 0))]:
            with self.subTest(tag=tag):
                self.assertEqual(updates.version(tag), expected)
        self.assertFalse(self.evaluate(['v0.45.0'], '0.45')['updateAvailable'])

    def test_missing_selected_line_and_bad_responses_fail_explicitly(self):
        with self.assertRaises(ValueError):
            self.evaluate(['n9.0.2'], '7.0.3', (7, 0))
        for response in ['{"message":"Not Found"}', '[{"name":null}]']:
            with self.subTest(response=response), self.assertRaises(ValueError):
                updates.decode_pages(response)

    def test_every_maintenance_source_and_embedded_release_is_covered(self):
        pins = json.loads((ROOT / 'dependencies.json').read_text())
        names = {row[0] for row in updates.checks(pins)}
        self.assertTrue({key.lower() for key in pins['maintenanceLibraries']['sources']} <= names)
        self.assertTrue({'sdk', 'opengl', 'pacbrew', 'json', 'mkpfs', 'evo', 'vlc-ps5-port'} <= names)

    def test_miniupnpc_never_uses_daemon_release_tags(self):
        result = updates.evaluate('miniupnpc', 'miniupnp/miniupnp', '2.3.3', None,
                                  [{'name': 'miniupnpd_2_3_11'}, {'name': 'miniupnpc_2_3_3'}])
        self.assertEqual(result['selectedTag'], 'miniupnpc_2_3_3')
        self.assertFalse(result['updateAvailable'])

    def test_odd_xz_and_fontconfig_development_versions_are_not_stable(self):
        self.assertIsNone(updates.version('v5.9.0', 'xz'))
        self.assertIsNone(updates.version('2.18.90', 'fontconfig'))
        self.assertEqual(updates.version('v5.8.4', 'xz'), (5, 8, 4))

    def test_official_gnu_index_excludes_signatures_and_other_projects(self):
        tags = updates.gnu_tags('<a href="libiconv-1.19.1.tar.gz">source</a><a href="libiconv-1.19.1.tar.gz.sig">sig</a><a href="other-9.0.tar.gz">other</a>')
        self.assertEqual(tags, [{'name': '1.19.1'}])


if __name__ == '__main__':
    unittest.main()
