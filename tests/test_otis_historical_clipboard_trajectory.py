"""The D36 copy remains one real clipboard source while its target grows."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def seconds(step):
    return sum(float(part) * 60 ** index for index, part in enumerate(reversed(step['videoTime'].split(':'))))


class HistoricalClipboardTrajectory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='otis-clipboard-trajectory-')
        cls.addClassCleanup(cls.directory.cleanup)
        cls.manifests = []
        for intro in [False, True]:
            folder = Path(cls.directory.name) / str(intro)
            target = folder / 'manifest.json'
            command = [sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                       '--workspace', str(folder / 'work'), '--output', str(target)]
            if intro:
                command.append('--include-intro')
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            cls.manifests.append(json.loads(target.read_text()))

    def test_original_d36_is_copied_once_before_the_target_grows(self):
        for manifest in self.manifests:
            with self.subTest(workspace=manifest['workspace']):
                interval = [s for s in manifest['steps'] if 1325.5 <= seconds(s) <= 1326.5]
                copied = [s for s in interval if s['action'].get('keys') == ['Ctrl+C']]
                self.assertEqual(len(copied), 1, 'Do not recopy/reselect the clipboard source after extending its target')
                self.assertEqual(seconds(copied[0]), 1325.5)
                self.assertFalse(any(s['action'].get('target') == 'formula.name' for s in interval),
                                 'Held source dimension phases cannot be replaced by completed Name Box ranges')

    def test_two_then_three_by_two_selection_remains_held_into_the_modal(self):
        for manifest in self.manifests:
            for time, count, keys in [(1325.5, '2R x 1C', ['Shift+Down']),
                                      (1326, '3R x 2C', ['Shift+Down', 'Shift+Right'])]:
                with self.subTest(workspace=manifest['workspace'], count=count):
                    held = [s for s in manifest['steps'] if seconds(s) == time
                            and s.get('expect', {}).get('nameBoxText') == count]
                    self.assertEqual(len(held), 1)
                    self.assertEqual(held[0]['action'], {'type': 'key_sequence', 'target': 'worksheet.grid',
                                                        'keys': keys, 'releaseModifiers': False})
                    self.assertIn(held[0]['evidence']['eventId'], ['history-0751', 'history-0752'])


if __name__ == '__main__':
    unittest.main()
