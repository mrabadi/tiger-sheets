"""Source-observed ordinary held dimensions remain live between modal checkpoints."""
import json
import copy
from pathlib import Path
import subprocess
import sys
import unittest
from otis_late_source_restore import restore_reviewed_late_source_records

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/validation/otis-90-minute'


class BalanceHeldSelectionPhases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_balance_manifest.py')],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        cls.steps = json.loads((BASE / 'balance-native-action-manifest.json').read_text())['steps']

    def require_count(self, time, count):
        rows = [s for s in self.steps if s['videoTime'] == time
                and s.get('expect', {}).get('nameBoxText') == count
                and s['action']['type']=='key_sequence']
        self.assertEqual(len(rows), 1, f'Original{time} visibly holds{count}; completed-range helpers omit it')
        self.assertIs(rows[0]['action'].get('releaseModifiers'), False)
        self.assertIn('physical', rows[0]['evidence'].get('executionMethod', '').lower())
        return rows[0]

    def test_ratio_rows_and_second_column_retain_each_observed_count(self):
        for time, count in [('54:10.366667','2R x 1C'),('54:10.533333','3R x 1C'),
                            ('54:10.666667','4R x 1C'),('54:10.800000','5R x 1C'),
                            ('54:11.000000','6R x 1C'),('54:11.166667','7R x 1C'),
                            ('54:11.300000','8R x 1C'),('54:11.800000','8R x 2C')]:
            with self.subTest(time=time): self.require_count(time, count)

    def test_historical_balance_rows_keep_the_observed_fast_drag_counts(self):
        for time, count in [('56:02.466667','7R x 1C'),('56:02.666667','10R x 1C'),
                            ('56:02.766667','16R x 1C'),('56:02.933333','18R x 1C'),
                            ('56:03.066667','20R x 1C'),('56:03.300000','22R x 1C'),
                            ('56:03.466667','24R x 1C')]:
            with self.subTest(time=time): self.require_count(time, count)

    def test_copied_balance_column_stays_held_through_all_five_extensions(self):
        for time, count in [('56:04.266667','24R x 2C'),('56:04.400000','24R x 3C'),
                            ('56:04.600000','24R x 4C'),('56:04.700000','24R x 5C'),
                            ('56:05.433333','24R x 6C')]:
            with self.subTest(time=time): self.require_count(time, count)
        copied = next(s for s in self.steps if s['id'] == 'balance-cashflow-197-copy-historical-balance-region')
        self.assertEqual(copied['videoTime'], '56:04.033333')
        self.assertEqual(copied['expect']['nameBoxText'], '24R x 1C')

    def test_release_precedes_fill_and_the_next_source_anchor(self):
        release = next((s for s in self.steps if s['videoTime'] == '54:12.166667'
                        and s['action']['type'] == 'modifier_release'), None)
        self.assertIsNotNone(release, 'Source releases to D22 before Ctrl+R changes E percentages')
        self.assertEqual(release['expect']['nameBoxText'], 'D22')
        filled = next(s for s in self.steps if s['id'] == 'balance-cashflow-135-fill-right')
        self.assertEqual(filled['videoTime'], '54:12.266667')
        selected = next(s for s in self.steps if s['id'] == 'balance-cashflow-136-select-G22')
        self.assertEqual(selected['videoTime'], '54:12.866667')

    def test_first_balance_drag_is_not_the_disproven_seven_row_copy(self):
        for time, count in [('56:00.366667','3R x 1C'),('56:00.433333','6R x 1C'),
                            ('56:00.466667','7R x 1C'),('56:00.500000','9R x 1C'),
                            ('56:00.533333','10R x 1C'),('56:00.600000','12R x 1C'),
                            ('56:00.666667','15R x 1C'),('56:00.700000','16R x 1C'),
                            ('56:00.733333','18R x 1C')]:
            with self.subTest(time=time): self.require_count(time, count)
        self.assertFalse(any(s['id'] == 'balance-cashflow-196-copy-seven-history-rows'
                             for s in self.steps), 'Dense originals show Ready without a copied seven-row block')
        released = next(s for s in self.steps if s['id'] == 'balance-cashflow-195-release-eighteen-rows')
        self.assertEqual(released['videoTime'], '56:00.866667')
        self.assertEqual(released['expect']['nameBoxText'], 'E58')
        anchor = next(s for s in self.steps if s['id'] == 'balance-cashflow-196-anchor-E59')
        self.assertEqual(anchor['videoTime'], '56:01.933333')

    def test_percent_row_and_rows_below_remain_held_before_modal(self):
        for time, count in [('63:12.266667','1R x 2C'),('63:12.333333','1R x 4C'),
                            ('63:12.366667','1R x 6C'),('63:12.433333','1R x 7C'),
                            ('63:12.566667','1R x 8C'),('63:13.366667','2R x 8C'),
                            ('63:13.766667','3R x 8C')]:
            with self.subTest(time=time): self.require_count(time, count)
        release = next(s for s in self.steps if s['id'] == 'balance-cashflow-439-release-percent-row')
        self.assertEqual(release['videoTime'], '63:12.933333')
        self.assertEqual(release['expect']['nameBoxText'], 'C31')

    def test_cashflow_copy_cancel_overshoots_and_widths_are_real_native_phases(self):
        for time, count in [('65:35.766667','6R x 1C'),('65:37.333333','9R x 1C'),
                            ('65:37.566667','11R x 1C'),('65:37.733333','14R x 1C'),
                            ('65:37.900000','19R x 1C'),('65:38.100000','21R x 1C'),
                            ('65:38.433333','22R x 1C'),('65:38.600000','23R x 1C'),
                            ('65:39.166667','24R x 1C'),('65:39.433333','23R x 1C'),
                            ('65:39.733333','23R x 2C'),('65:40.000000','23R x 1C'),
                            ('65:40.600000','23R x 2C'),('65:40.766667','23R x 3C'),
                            ('65:40.900000','23R x 4C'),('65:41.100000','23R x 5C'),
                            ('65:41.266667','23R x 6C')]:
            with self.subTest(time=time): self.require_count(time, count)
        previous = next(s for s in self.steps if s['id'] == 'balance-cashflow-551-copy-E100')
        self.assertEqual(previous['videoTime'], '65:35.000000')
        cancel = next(s for s in self.steps if s['id'] == 'balance-cashflow-552-cancel-previous-copy')
        self.assertEqual(cancel['action']['keys'], ['Escape'])
        self.assertEqual(cancel['expect']['nameBoxText'], 'E98')
        copied = next(s for s in self.steps if s['id'] == 'balance-cashflow-556-copy-full-cashflow-history-formats')
        self.assertEqual(copied['videoTime'], '65:40.366667')
        self.assertEqual(copied['expect']['nameBoxText'], '23R x 1C')

    def test_source_scroll_pixels_preserve_the_released_and_held_selection(self):
        for time, row, offset in [('56:01.233333',57,-14),('56:01.266667',58,-2),
                                  ('56:01.300000',58,-13),('56:01.366667',58,-16),
                                  ('56:01.400000',58,-20),('56:01.433333',59,0),
                                  ('65:36.733333',89,-15),('65:36.800000',95,0),
                                  ('65:38.600000',96,-10),('65:39.366667',98,0)]:
            with self.subTest(time=time):
                matches = [s for s in self.steps if s['videoTime'] == time
                           and s['action'] == {'type':'vertical_view','topRow':row,'topRowOffset':offset}]
                self.assertEqual(len(matches),1,'Every original viewport transition retains its actual partial-row geometry')
                self.assertIn('nameBoxText',matches[0]['expect'])

    def test_only_reviewed_followup_events_change_and_children_and_order_matter(self):
        before = json.loads((BASE / 'post-attempt9-balance-held-selection-followup/before/balance-native-action-manifest.json').read_text())['steps']
        owned = {'balance-cashflow-'+str(n) for n in
                 [133,134,135,136,194,195,196,197,198,199,439,440,441,551,552,553,554,555,556,557]}
        def outside(records):
            return [s for s in records if s['evidence']['eventId'] not in owned]
        expected = outside(before)
        self.assertEqual(outside(restore_reviewed_late_source_records(self.steps)), expected)
        damaged = copy.deepcopy(self.steps)
        modal = next(s for s in damaged if s['evidence']['eventId'] not in owned
                     and s['action']['type']=='modal')
        modal['action']['steps'][0]['action']={'type':'key_sequence','target':'@active_modal','keys':['Escape']}
        with self.assertRaisesRegex(AssertionError,'Unapproved action/order: exact reviewed adjacent after-image required'):
            restore_reviewed_late_source_records(damaged)
        reordered = copy.deepcopy(expected)
        reordered[0],reordered[1]=reordered[1],reordered[0]
        self.assertNotEqual(reordered,expected,'Outside-event record order remains exact')


if __name__ == '__main__':
    unittest.main()
