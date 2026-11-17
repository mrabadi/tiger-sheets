"""Source held-selection feedback must survive the real width dialog."""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/validation/otis-90-minute'


class IncomeModalHeaders(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_income_manifest.py')], cwd=ROOT, check=True, capture_output=True)
        cls.steps = json.loads((BASE / 'income-native-action-manifest.json').read_text())['steps']

    def test_return_to_G4_uses_first_observed_G4_header(self):
        ledger = json.loads((BASE / 'income-width-modal-phases.json').read_text())
        phase = next(p for p in ledger['groups']['income-150'] if p['id'] == 'width-return-G4')
        self.assertEqual(phase['videoTime'], '42:38.600000',
                         'The retained42:38.566667frame still shows F4; G4 first appears in the next native source frame')
        frame = BASE / 'source-frames/income-width-151-boundaries/2558.600000.png'
        self.assertEqual(phase['sourceFrame'], str(frame))
        self.assertEqual(hashlib.sha256(frame.read_bytes()).hexdigest(),
                         '691d52122e383ff5a29b13565621e1b60e0669f4dc4199f5fff9997403772bbe')
        self.assertIn(str(frame), [p['path'] for p in ledger['originalFrames']])
        action = next(p for p in self.steps if p['id'] == 'income-150-width-return-G4')
        self.assertEqual(action['videoTime'], phase['videoTime'])
        self.assertEqual(action['expect']['nameBoxText'], 'G4')
        self.assertEqual(action['evidence']['frames'], [str(frame)])

    def test_four_column_header_remains_held_inside_width_dialog(self):
        modal = next(s for s in self.steps if s['id'] == 'income-151-column-width-dialog')
        children = modal['action']['steps']
        selected = next(s for s in children if s['action'].get('keys') == ['Ctrl+A'])
        self.assertEqual(selected.get('expect', {}).get('nameBoxText'), '1R x 4C', 'Original42:41 width dialog retains the actual live selection count')
        typed = [s for s in children if s['action']['type'] == 'text']
        self.assertTrue(typed)
        self.assertTrue(all(s.get('expect', {}).get('nameBoxText') == '1R x 4C' for s in typed))
        self.assertEqual([s['action']['text'] for s in typed], ['1', '1', '.', '1', '4'])
        self.assertTrue(children[-1]['action']['keys'] == ['Return'])
        self.assertEqual(modal['expect']['nameBoxText'], '1R x 4C', 'The count remains visible after the source dialog closes')
        following = [s for s in self.steps if s['evidence']['eventId'] == 'income-151' and s['action']['type'] != 'modal']
        releases = [s for s in following if s['action']['type'] == 'modifier_release']
        self.assertEqual(len(releases), 1)
        self.assertEqual(releases[0]['expect']['nameBoxText'], 'G4')
        self.assertGreater(releases[0]['videoTime'], children[-1]['videoTime'])
        self.assertEqual([s['expect']['nameBoxText'] for s in following if s['action'].get('keys') in [['Left'], ['Down']]], ['F4', 'F5', 'F6', 'F7'])


if __name__ == '__main__':
    unittest.main()
