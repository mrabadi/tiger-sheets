"""Original40–50min modal headers and radio choices are native replay contracts."""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/validation/otis-90-minute'

class IncomePasteModalCounts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_income_manifest.py')], cwd=ROOT, check=True, capture_output=True)
        cls.steps = json.loads((BASE / 'income-native-action-manifest.json').read_text())['steps']

    def assert_held_modal(self, event, header, row, column, mode):
        records = [s for s in self.steps if s['evidence']['eventId'] == event]
        modal = next(s for s in records if s['action']['type'] == 'modal')
        children = modal['action']['steps']
        chosen = next(s for s in children if s['action'].get('keys') == [mode])
        self.assertEqual(chosen.get('expect', {}).get('nameBoxText'), header,
                         'The original actual modal retains the live selection count')
        self.assertEqual((chosen['expect']['row'], chosen['expect']['column']), (row, column))
        held = [s for s in records if s['action'].get('releaseModifiers') is False]
        self.assertTrue(held, 'A genuine native held selection must produce the count')
        self.assertTrue(any(s.get('expect', {}).get('nameBoxText') == header for s in held))
        release = next(s for s in children if s['action']['type'] == 'modifier_release')
        self.assertEqual(release['expect']['nameBoxText'], f'{chr(64+column)}{row}')
        self.assertGreater(children.index(release), children.index(chosen))
        self.assertEqual(children[-1]['action']['keys'], ['Return'])
        self.assertTrue(all(s['action'].get('gesture') != 'release' for s in records[:records.index(modal)]),
                        'The destination must remain held until its native modal has appeared')

    def test_141_three_forecast_growth_cells_remain_held_in_paste_dialog(self):
        self.assert_held_modal('income-141', '1R x 3C', 5, 8, 'T')
        copied = next(i for i, s in enumerate(self.steps) if s['evidence']['eventId'] == 'income-141'
                      and s['action'].get('keys') == ['Ctrl+C'])
        self.assertEqual(self.steps[copied-1]['action'], {'type': 'mouse_cell', 'address': 'G5', 'gesture': 'click'},
                         'Original42:18.5 active/copy source is G5, even though F5 has the same format')

    def test_153_five_market_share_cells_remain_held_in_paste_dialog(self):
        self.assert_held_modal('income-153', '1R x 5C', 7, 6, 'T')

    def test_159_source_column_is_included_in_nineteen_by_six_destination(self):
        self.assert_held_modal('income-159', '19R x 6C', 19, 5, 'T')

    def test_237_three_average_formulas_remain_held_in_paste_dialog(self):
        self.assert_held_modal('income-237', '3R x 1C', 15, 6, 'F')

    def test_238_fifteen_average_cells_remain_held_in_formats_dialog(self):
        self.assert_held_modal('income-238', '3R x 5C', 15, 6, 'T')

    def test_249_both_tax_and_nci_rows_are_in_the_held_destination(self):
        self.assert_held_modal('income-249', '2R x 8C', 19, 3, 'T')

    def test_260_other_income_to_pension_pastes_formulas_not_all(self):
        modal = next(s for s in self.steps if s['id'] == 'income-260-paste-special-dialog')
        choices = [s['action']['keys'] for s in modal['action']['steps'] if s['action'].get('keys') in [['A'], ['F'], ['T']]]
        self.assertEqual(choices, [['F']], 'Original49:35.4 visibly checks Formulas, while All is empty')

    def test_existing_151_width_source_contract_is_unchanged(self):
        baseline = json.loads((BASE / 'post-attempt9-income-paste-modal-counts/before/income-native-action-manifest.json').read_text())['steps']
        expected = [s for s in baseline if s['evidence']['eventId'] in ['income-150', 'income-151']]
        # The separate width owner corrected exactly one independently decoded frame.
        # Preserve every other owner field instead of accepting a new broad baseline.
        delta_file = BASE / 'canonical-attempt9-prelaunch-source-hazards/independent-width-151-review/exact-one-phase-delta.json'
        self.assertEqual(hashlib.sha256(delta_file.read_bytes()).hexdigest(),
                         '3d69e6b0be81a4af1b7f44ae3232f2fe7fbfc1055dff18f7450a7166a5272a78')
        corrections = json.loads(delta_file.read_text())['changedRecursiveRecords']
        self.assertEqual(len(corrections), 1)
        correction = corrections[0]
        index = next(i for i, s in enumerate(expected) if s['id'] == correction['id'])
        self.assertEqual(expected[index], correction['before'])
        expected[index] = correction['after']
        actual = [s for s in self.steps if s['evidence']['eventId'] in ['income-150', 'income-151']]
        self.assertEqual(actual, expected, 'The independently frozen151 width phases are outside this repair')

if __name__ == '__main__':
    unittest.main()
