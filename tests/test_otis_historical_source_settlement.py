"""Observed retained cells, held headers and settled values need real native guards."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'docs/validation/otis-90-minute'


class HistoricalSourceSettlementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        folder = Path(cls.temporary.name)
        output = folder / 'history.json'
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                        '--include-intro', '--workspace', str(folder / 'work'),
                        '--output', str(output)], check=True, capture_output=True)
        cls.manifest = json.loads(output.read_text())
        cls.steps = cls.manifest['steps']
        cls.by_id = {r['id']: r for r in cls.steps}

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_c24_commit_and_held_extension_preserve_all_five_original_action_ids(self):
        commit = self.by_id['historical-01779-commit-local-pointed-formula']
        self.assertEqual(commit['videoTime'], '15:38.200')
        self.assertEqual(commit['action'], {'type': 'key_sequence',
                                          'target': 'formula.input', 'keys': ['Ctrl+Return']})
        self.assertEqual((commit['expect']['row'], commit['expect']['column'],
                          commit['expect']['nameBoxText']), (24, 3, 'C24'))
        expected = [
            ('historical-01780-keys', '15:38.467',
             {'type': 'key_sequence', 'target': 'worksheet.grid',
              'keys': ['Shift+Right'], 'releaseModifiers': False}, '1R x 2C'),
            ('historical-01781-address', '15:38.600',
             {'type': 'key_sequence', 'target': 'worksheet.grid',
              'keys': ['Shift+Right'], 'releaseModifiers': False}, '1R x 3C'),
            ('historical-01782-select', '15:39.133',
             {'type': 'modifier_release', 'target': 'worksheet.grid', 'keys': ['Shift']}, 'C24'),
            ('historical-01783-fill-right', '15:39.133',
             {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Ctrl+R']}, 'C24'),
        ]
        for identity, time, action, header in expected:
            with self.subTest(identity=identity):
                record = self.by_id[identity]
                self.assertEqual(record['videoTime'], time)
                self.assertEqual(record['action'], action)
                self.assertEqual((record['expect']['row'], record['expect']['column'],
                                  record['expect']['nameBoxText']), (24, 3, header))
                self.assertTrue(Path(record['evidence']['sourceFrame']).is_file())
                self.assertIn('inferred', record['evidence']['replayMethod'].lower())
        self.assertLess(self.steps.index(self.by_id[expected[2][0]]),
                        self.steps.index(self.by_id[expected[3][0]]))
        cells = {r['address']: r for r in self.by_id[expected[3][0]]['expect']['cells']}
        for column, serial, display in [('C', 43830, 'FY 19'), ('D', 44196, 'FY 20'),
                                        ('E', 44561, 'FY 21')]:
            self.assertEqual(cells[column + '24'], {'address': column + '24',
                             'input': '=' + column + '2', 'value': serial,
                             'valueType': 'number', 'display': display})

    def test_source_autofit_keeps_b55_and_top43_without_a_fabricated_b1_jump(self):
        self.assertEqual(self.by_id['historical-03339-address']['action'],
                         {'type': 'text', 'target': 'formula.name', 'text': 'B55'})
        prep = self.by_id['historical-03341-whole-columns']
        self.assertEqual(prep['action'],
                         {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': []})
        expected = {'row': 55, 'column': 2, 'nameBoxText': 'B55', 'topRow': 43,
                    'topRowOffset': 0}
        self.assertEqual(prep['expect'], expected)
        fit = self.by_id['historical-03342-keys']
        self.assertEqual(fit['action'], {'type': 'key_sequence', 'target': 'worksheet.grid',
                                        'keys': ['Alt', 'O', 'C', 'A']})
        self.assertEqual(fit['expect'], expected)

    def test_observed_formula_results_settle_before_existing_edit_captures(self):
        committed = self.by_id['historical-02210-commit-reference']
        c30 = next(c for c in committed['expect']['cells'] if c['address'] == 'C30')
        self.assertEqual(c30.get('value'), 3031,
                         'The source already displays the calculated external total before F2')
        self.assertEqual(c30.get('valueType'), 'number')
        record = self.by_id['historical-03367-commit']
        self.assertEqual(record['action'], {'type': 'key_sequence',
                                          'target': 'worksheet.cellEditor', 'keys': ['Ctrl+Return']})
        self.assertEqual(record['videoTime'], '28:03.933')
        self.assertEqual({key: record['expect'][key] for key in ['row', 'column', 'nameBoxText']},
                         {'row': 57, 'column': 2, 'nameBoxText': 'B57'})
        cells = {c['address']: c for c in record['expect']['cells']}
        for address, value in [('D44', -3862), ('E44', -3625), ('D46', -3201),
                               ('E46', -2984), ('D48', 10710), ('E48', 12279),
                               ('D50', 0), ('E50', 0)]:
            self.assertEqual(cells.get(address), {'address': address, 'value': value,
                                                'valueType': 'number'})
        self.assertEqual(cells.get('C52'), {'address': 'C52', 'value': 43830,
                                          'valueType': 'number', 'display': 'FY 19'})
        self.assertEqual(cells['B57']['input'], 'Pensions/Other Items:')

    def test_existing_c30_inspection_keeps_uniform_sum_format_and_settled_warning_values(self):
        record = self.by_id['historical-02214-keys']
        self.assertEqual(record['action'], {'type': 'key_sequence',
                                          'target': 'worksheet.grid', 'keys': ['F2']})
        cells = {c['address']: c for c in record['expect']['cells']}
        self.assertEqual(cells.get('C27'), {'address': 'C27', 'value': 1799,
                                          'valueType': 'number', 'display': '1799'})
        self.assertEqual(cells.get('C29'), {'address': 'C29', 'value': 1546,
                                          'valueType': 'number', 'display': '1,546',
                                          'style': {'numberFormat': '#,##0'}})

    def test_b59_held_headers_use_existing_ids_and_do_not_invent_a_sampled_three_columns(self):
        expected = [
            ('historical-04748-source-insert', '28:46.467', 1, '1R x 2C'),
            ('historical-04749-source-insert', '28:47.000', 2, '1R x 4C'),
            ('historical-04750-source-insert', '28:47.033', 1, '1R x 5C'),
            ('historical-04751-source-insert', '28:47.067', 1, '1R x 6C'),
            ('historical-04752-source-insert', '28:47.100', 1, '1R x 7C'),
            ('historical-04753-source-insert', '28:47.133', 1, '1R x 8C'),
            ('historical-03422-keys', '28:47.200', 1, '1R x 9C'),
        ]
        for identity, time, right_count, header in expected:
            with self.subTest(identity=identity):
                record = self.by_id[identity]
                self.assertEqual(record['videoTime'], time)
                self.assertEqual(record['action'], {'type': 'key_sequence',
                                 'target': 'worksheet.grid', 'keys': ['Shift+Right'] * right_count,
                                 'releaseModifiers': False})
                self.assertEqual(record['expect']['nameBoxText'], header)
                self.assertEqual((record['expect']['row'], record['expect']['column'],
                                  record['expect']['topRow']), (59, 2, 43))
                self.assertIn('inferred', record['evidence']['replayMethod'].lower())
        held = self.by_id['historical-03423-address']
        self.assertEqual(held['videoTime'], '28:47.500')
        self.assertEqual(held['action'], {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': []})
        self.assertEqual(held['expect']['nameBoxText'], '1R x 9C')
        release = self.by_id['historical-03424-select']
        self.assertEqual(release['videoTime'], '28:48.867')
        self.assertEqual(release['action'], {'type': 'modifier_release',
                         'target': 'worksheet.grid', 'keys': ['Shift']})
        self.assertEqual(release['expect']['nameBoxText'], 'B59')
        self.assertFalse(any(r.get('expect', {}).get('nameBoxText') == '1R x 3C'
                             for r in self.steps if '28:46.000' <= r['videoTime'] < '28:49.000'))

    def test_cashflow_reference_switches_verify_the_actual_source_before_wheeling(self):
        for identity in ['historical-03620-switch-source', 'historical-03632-switch-source']:
            with self.subTest(identity=identity):
                record = self.by_id[identity]
                self.assertEqual(record['action'], {'type': 'key_sequence',
                                 'target': 'worksheet.cellEditor', 'keys': ['Ctrl+Shift+Tab']})
                self.assertEqual(record.get('expect', {}).get('documentName'),
                                 'Otis-10-K-Raw.xls',
                                 'A real source-window check must precede its source-cell wheel action')

    def test_currency_popup_accepts_its_choice_before_separate_dialog_acceptance(self):
        def descend(records):
            for record in records:
                yield record
                yield from descend(record.get('action', {}).get('steps', []))
        nested = {r['id']: r for r in descend(self.steps)}
        folder = Path(self.temporary.name)
        output = folder / 'default-symbol.json'
        result = subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                                 '--workspace', str(folder / 'default-symbol-work'), '--output', str(output)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        default_records = list(descend(json.loads(output.read_text())['steps']))
        for choice_id, accept_id, choice_time, accept_time, choice_keys in [
                ('historical-01729-dialog-action', 'historical-01730-dialog-action',
                 '15:02.000', '15:02.500', ['Home', 'Down', 'Return']),
                ('historical-02266-dialog-action', 'historical-02267-dialog-action',
                 '19:17.500', '19:17.500', ['Home', 'Down', 'Return']),
                ('historical-02787-dialog-action', 'historical-02788-dialog-action',
                 '23:38.000', '23:38.500', ['Home', 'Return'])]:
            with self.subTest(choice=choice_id):
                choice, accept = nested[choice_id], nested[accept_id]
                self.assertEqual(choice['action'], {'type': 'key_sequence',
                                 'target': 'formatCells.currencySymbol',
                                 'keys': choice_keys},
                                 'Original source already closes the currency popup before dialog acceptance')
                self.assertEqual(choice['videoTime'], choice_time)
                self.assertEqual(accept['videoTime'], accept_time)
                self.assertEqual(accept['action'], {'type': 'key_sequence',
                                 'target': 'formatCells.dialog', 'keys': ['Return']})
                self.assertIn('inferred', choice['evidence']['replayMethod'].lower())
                actual = [r for r in default_records if r['videoTime'] == choice_time
                          and r['action'].get('target') == 'formatCells.currencySymbol']
                self.assertEqual(len(actual), 1)
                self.assertEqual(actual[0]['action'], choice['action'])

    def test_source_window_guards_also_preserve_default_two_window_replay(self):
        folder = Path(self.temporary.name)
        output = folder / 'default.json'
        result = subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                                 '--workspace', str(folder / 'default-work'), '--output', str(output)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        records = json.loads(output.read_text())['steps']
        switches = [r for r in records if r['id'].endswith('-switch-source')
                    and r['videoTime'] in ['30:15.500', '30:26.500']]
        self.assertEqual(len(switches), 2)
        for record in switches:
            self.assertEqual(record['action']['keys'], ['Ctrl+Tab'])
            self.assertEqual(record['expect']['documentName'], 'Otis-10-K-Raw.xls')


if __name__ == '__main__':
    unittest.main()
