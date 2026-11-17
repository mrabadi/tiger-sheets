import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

def seconds(value):
    return sum(float(part) * 60 ** index for index, part in enumerate(reversed(value.split(':'))))

class LinkingVisibleLabelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_linking_manifest.py')], cwd=ROOT, check=True, capture_output=True)
        cls.steps = json.loads((ROOT / 'docs/validation/otis-90-minute/linking-native-action-manifest.json').read_text())['steps']

    def draft_at(self, start, time):
        draft = None
        for step in self.steps:
            if not seconds(start) <= seconds(step['videoTime']) <= seconds(time) + 0.0005:
                continue
            action = step['action']
            if action['type'] == 'text':
                if action.get('target') == 'worksheet.grid':
                    draft = action['text']
                elif action.get('target') == 'worksheet.cellEditor' and draft is not None:
                    draft += action['text']
            elif action['type'] == 'key_sequence' and action.get('target') == 'worksheet.cellEditor':
                for key in action['keys']:
                    if key == 'Backspace' and draft is not None:
                        draft = draft[:-1]
                    elif key in ['Return', 'Ctrl+Return', 'Tab', 'Shift+Return']:
                        draft = None
        return draft

    def phases(self, start, expected):
        for time, text in expected:
            with self.subTest(time=time, text=text):
                self.assertEqual(self.draft_at(start, time), text)

    def committed(self, address, time, key, row, column, value):
        candidates = [s for s in self.steps if abs(seconds(s['videoTime']) - seconds(time)) < .001 and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.cellEditor', 'keys': [key]}]
        self.assertEqual(len(candidates), 1, (address, time, key))
        step = candidates[0]
        self.assertEqual(step['expect']['row'], row)
        self.assertEqual(step['expect']['column'], column)
        self.assertIn({'address': address, 'input': value}, step['expect']['cells'])
        self.assertIn('physical', step['evidence']['executionMethod'].lower())

    def later_down(self, time, address):
        candidates = [s for s in self.steps if s['videoTime'] == time
                      and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Down']}]
        self.assertEqual(len(candidates), 1, 'Later navigation is a distinct source phase from first Ready')
        self.assertEqual(candidates[0]['expect']['nameBoxText'], address)

    def test_minimum_cash_typo_correction_and_commit(self):
        self.phases('72:05', [('72:05.5', 'M'), ('72:06', 'Minim'), ('72:06.5', 'Minimum C'), ('72:07', 'Minimum Csah:'), ('72:07.5', 'Minimum Csah'), ('72:08', 'Minimum C'), ('72:08.5', 'Minimum Cash:')])
        self.committed('B37', '72:08.633333', 'Ctrl+Return', 37, 2, 'Minimum Cash:')
        self.later_down('72:08.866667', 'B38')

    def test_excess_cash_visible_prefixes_and_commit(self):
        self.phases('72:09', [('72:09.5', 'Exce'), ('72:10', 'Excess'), ('72:10.5', 'Excess Cash'), ('72:11', 'Excess Cash Flow'), ('72:11.1', 'Excess Cash Flow:')])
        self.committed('B38', '72:11.166667', 'Ctrl+Return', 38, 2, 'Excess Cash Flow:')
        self.later_down('72:11.400000', 'B39')

    def test_stock_label_typo_is_not_committed_early(self):
        self.phases('72:14', [('72:14', '%'), ('72:14.5', '% S'), ('72:15', '% Stock'), ('72:15.5', '% Stock R'), ('72:16', '% Stock Repru'), ('72:16.5', '% Stock Reprucha'), ('72:17', '% Stock Repru'), ('72:17.5', '% Stock Repu'), ('72:18', '% Stock Repurc'), ('72:18.5', '% Stock Repurchases'), ('72:18.700000', '% Stock Repurchases:')])
        self.committed('B39', '72:18.733333', 'Ctrl+Return', 39, 2, '% Stock Repurchases:')
        self.assertIsNone(self.draft_at('72:14', '72:18.733333'))
        self.later_down('72:19.033333', 'B40')

    def test_debt_label_returns_to_previous_row(self):
        self.phases('72:19.4', [('72:19.5', '%'), ('72:20', '% Debt'), ('72:20.5', '% Debt R'), ('72:21', '% Debt Repaym'), ('72:21.5', '% Debt Repayment:')])
        self.committed('B40', '72:21.7', 'Shift+Return', 39, 2, '% Debt Repayment:')

    def test_allocation_range_retains_observed_live_dimension_until_release(self):
        held = [s for s in self.steps if s['videoTime'] == '72:22' and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Shift+Down'], 'releaseModifiers': False}]
        self.assertEqual(len(held), 1, 'The stationary source pointer and live header require a real held selection phase')
        self.assertEqual(held[0]['expect']['nameBoxText'], '2R x 1C')
        self.assertEqual(held[0]['expect']['row'], 39)
        self.assertEqual(held[0]['expect']['column'], 2)
        released = [s for s in self.steps if s['videoTime'] == '72:22.8' and s['action'] == {'type': 'modifier_release', 'target': 'worksheet.grid', 'keys': ['Shift']}]
        self.assertEqual(len(released), 1)
        self.assertEqual(released[0]['expect']['nameBoxText'], 'B39')
        self.assertFalse(any(s['action'].get('type') == 'mouse_cell' for s in self.steps if seconds('72:22') <= seconds(s['videoTime']) <= seconds('72:22.8')), 'Do not replace the observed stationary-pointer phase with a drag')
        for time, row in [('72:22.833333', 40), ('72:22.9', 41)]:
            navigated = [s for s in self.steps if s['videoTime'] == time and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Down']}]
            self.assertEqual(len(navigated), 1, 'Retain both observed active-cell changes after the held selection')
            self.assertEqual(navigated[0]['expect']['nameBoxText'], f'B{row}')

    def test_other_items_colon_removal_and_delayed_suffix(self):
        self.phases('74:48', [('74:48.5', 'Other Items:'), ('74:49.5', 'Other Items:'), ('74:50', 'Other Items'), ('74:54', 'Other Items'), ('74:54.5', 'Other Items % De'), ('74:55', 'Other Items % Debt I'), ('74:55.5', 'Other Items % Debt Issu'), ('74:56', 'Other Items % Debt Issuances'), ('74:56.066667', 'Other Items % Debt Issuances:')])
        self.committed('B47', '74:56.4', 'Tab', 47, 3, 'Other Items % Debt Issuances:')

    def test_interest_label_has_actual_ep_typo_and_tab_commit(self):
        self.phases('76:32', [('76:32.5', 'I'), ('76:33', 'Interest'), ('76:33.5', 'Interest Exp'), ('76:34', 'Interest Ep'), ('76:34.5', 'Interest Expe'), ('76:35', 'Interest Expense'), ('76:35.5', 'Interest Expense %'), ('76:36', 'Interest Expense % Debt'), ('76:36.133333', 'Interest Expense % Debt:')])
        self.committed('B42', '76:36.4', 'Tab', 42, 3, 'Interest Expense % Debt:')

    def test_dividend_scratch_inspection_is_ready_without_invented_f2(self):
        interval = [s for s in self.steps if seconds('81:06') <= seconds(s['videoTime']) <= seconds('81:09')]
        self.assertFalse(any(k in ['F2', 'Return'] for s in interval for k in s['action'].get('keys', [])), 'Original105frames show Ready and value-only F111, so do not synthesize an edit/commit')
        held = [s for s in interval if s['videoTime'] == '81:09' and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': []}]
        self.assertEqual(len(held), 1)
        self.assertEqual(held[0]['expect']['nameBoxText'], 'F111')
        self.assertIn({'address': 'F111', 'input': '=SUM(F118:H118)'}, held[0]['expect']['cells'])

    def test_debt_accounting_modal_retains_live_dimensions_and_visible_choices(self):
        modal = next(s for s in self.steps if s['id'] == 'linking-debt-checks-198-paste-special-dialog')
        choices = [s for s in modal['action']['steps'] if s['action'].get('keys') in [['T'], ['C']]]
        self.assertEqual([s['action']['keys'] for s in choices], [['T'], ['C'], ['T']], 'Original74:15.133/15.300/15.466 shows Formats, Comments and Notes, then Formats')
        for choice in choices:
            self.assertEqual(choice['expect']['nameBoxText'], '1R x 5C')
            self.assertEqual((choice['expect']['row'], choice['expect']['column']), (42, 6))
        released = [s for s in modal['action']['steps'] if s['action']['type'] == 'modifier_release']
        self.assertEqual(len(released), 1)
        self.assertEqual(released[0]['videoTime'], '74:16.733333')
        self.assertEqual(released[0]['expect']['nameBoxText'], 'F42')
        for time, header in [('74:13.733333', '1R x 2C'), ('74:13.900000', '1R x 3C'), ('74:14.100000', '1R x 4C'), ('74:14.300000', '1R x 5C')]:
            matches = [s for s in self.steps if s['videoTime'] == time and s.get('expect', {}).get('nameBoxText') == header]
            self.assertEqual(len(matches), 1, (time, header))
            self.assertFalse(matches[0]['action']['releaseModifiers'])
        for time, key, header in [('74:16.933333', 'Down', 'F43'), ('74:17.133333', 'Up', 'F42')]:
            matches = [s for s in self.steps if s['videoTime'] == time and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': [key]}]
            self.assertEqual(len(matches), 1)
            self.assertEqual(matches[0]['expect']['nameBoxText'], header)

    def test_allocation_formats_modal_retains_two_row_header_until_close(self):
        modal = next(s for s in self.steps if s['id'] == 'linking-debt-checks-188-paste-special-dialog')
        choice = next(s for s in modal['action']['steps'] if s['action'].get('keys') == ['T'])
        self.assertEqual(choice.get('expect', {}).get('nameBoxText'), '2R x 1C')
        released = [s for s in modal['action']['steps'] if s['action']['type'] == 'modifier_release']
        self.assertEqual(len(released), 1)
        self.assertEqual(released[0]['videoTime'], '74:00.933333')
        self.assertEqual(released[0]['expect']['nameBoxText'], 'E39')
        held = [s for s in self.steps if s['videoTime'] == '74:00.233333' and s['action'] == {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Shift+Down'], 'releaseModifiers': False}]
        self.assertEqual(len(held), 1)
        self.assertEqual(held[0]['expect']['nameBoxText'], '2R x 1C')
        self.assertGreaterEqual(seconds(modal['videoTime']), seconds(held[0]['videoTime']), 'The native modal must open after the source-held selection, even when both share one coarse ledger second')

if __name__ == '__main__':
    unittest.main()
