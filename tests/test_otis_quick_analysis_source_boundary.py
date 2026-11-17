"""The source QA icon appears after release, not in the held 14:10 frame."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'docs/validation/otis-90-minute'


class QuickAnalysisSourceBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        folder = Path(cls.temporary.name)
        output = folder / 'history.json'
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                        '--workspace', str(folder / 'work'), '--output', str(output)],
                       check=True, capture_output=True)
        cls.steps = json.loads(output.read_text())['steps']

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_held_frame_is_an_actual_negative_visibility_checkpoint(self):
        matches = [r for r in self.steps if r['videoTime'] == '14:10.000'
                   and r['evidence'].get('eventId') == 'history-0321'
                   and r['action']['type'] == 'key_sequence']
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]['action'],
                         {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': []},
                         'The original held frame has no QA widget to target')
        self.assertEqual(matches[0]['expect'],
                         {'row': 23, 'column': 7, 'nameBoxText': '1R x 3C',
                          'quickAnalysisVisible': False})

    def test_release_and_first_icon_have_separate_source_bounded_checkpoints(self):
        release = [r for r in self.steps if r['action'] ==
                   {'type': 'mouse_cell', 'address': 'I23', 'gesture': 'release'}
                   and r['evidence'].get('phaseGroup') == 'live-selection-raw-net-income']
        self.assertEqual(len(release), 1)
        self.assertEqual(release[0]['videoTime'], '14:10.300',
                         'G23 replaces the held count in the first native frame850.300')
        self.assertEqual(release[0]['expect'], {'row': 23, 'column': 7, 'nameBoxText': 'G23'})
        visible = [r for r in self.steps if r['videoTime'] == '14:10.400'
                   and r.get('expect', {}).get('quickAnalysisVisible') is True]
        self.assertEqual(len(visible), 1, 'First actual icon appearance is independently verified')
        self.assertEqual(visible[0]['action'],
                         {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': []})
        self.assertEqual(visible[0]['expect'], {'row': 23, 'column': 7,
                                              'nameBoxText': 'G23',
                                              'quickAnalysisVisible': True})
        self.assertLess(self.steps.index(release[0]), self.steps.index(visible[0]))
        for record in [release[0], visible[0]]:
            self.assertTrue(Path(record['evidence']['sourceFrame']).is_file())
        self.assertFalse(any(r['videoTime'] == '14:10.700' for r in self.steps),
                         'A pre-switch disappearance does not authorize an invented hide gesture')

    def test_corrected_count_observation_has_an_individual_surface_review(self):
        review = json.loads((EVIDENCE / 'historical-live-selection-phases.json').read_text())
        matches = [r for r in review['countObservationReview']
                   if r['sourceEventId'] == 'history-0321']
        self.assertEqual(len(matches), 1,
                         'The now explicit1R x3C source observation needs its own reviewed surface')
        self.assertEqual(matches[0]['sourceNameBoxText'], '1R x 3C')
        self.assertEqual(matches[0]['surface'], 'name-box-held-dimensions')
        self.assertFalse(matches[0]['canonicalExecutionComplete'])


if __name__ == '__main__':
    unittest.main()
