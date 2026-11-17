"""Actual Ready origins for the independently reviewed formula-bar commits."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'docs/validation/otis-90-minute'
REVIEW = EVIDENCE / 'post-attempt12-bar33-first-ready-review/review.json'


class BarReadyCommitHeaders(unittest.TestCase):
    def test_all_thirty_three_actual_ready_origins_are_retained(self):
        self.assertEqual(hashlib.sha256(REVIEW.read_bytes()).hexdigest(),
                         '0de4f707bcd788c911df0b865615b14bdf9754c04ddb5e519ce6a5bba532c11c')
        rows = json.loads(REVIEW.read_text())['rows']
        self.assertEqual(len(rows), 33)
        with tempfile.TemporaryDirectory(prefix='otis-bar33-contract-') as folder:
            for mode in ['include-intro', 'default']:
                output = Path(folder) / (mode + '.json')
                command = [sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                           '--workspace', str(Path(folder) / (mode + '-work')),
                           '--output', str(output)]
                if mode == 'include-intro':
                    command.append('--include-intro')
                subprocess.run(command, check=True, capture_output=True)
                steps = json.loads(output.read_text())['steps']
                for row in rows:
                    with self.subTest(mode=mode, origin=row['origin'], source=row['currentId']):
                        expected_cells = row['currentRecord']['expect']['cells']
                        candidates = [step for step in steps
                                      if step['action'].get('target') == 'formula.input'
                                      and step['action']['type'] == 'key_sequence'
                                      and all(any(all(cell.get(key) == value for key, value in original.items())
                                                  for cell in step.get('expect', {}).get('cells', []))
                                              for original in expected_cells)]
                        self.assertEqual(len(candidates), 1)
                        commit = candidates[0]
                        self.assertEqual(commit['action']['keys'], ['Ctrl+Return'])
                        seconds = row['firstCoherentHeaderAndBar']['seconds']
                        self.assertEqual(commit['videoTime'], f'{int(seconds // 60):02d}:{seconds % 60:06.3f}')
                        column_name, number = re.fullmatch(r'([A-Z]+)([1-9][0-9]*)', row['origin']).groups()
                        column = 0
                        for letter in column_name:
                            column = column * 26 + ord(letter) - 64
                        self.assertEqual({key: commit['expect'].get(key)
                                          for key in ['row', 'column', 'nameBoxText']},
                                         {'row': int(number), 'column': column, 'nameBoxText': row['origin']})
                        self.assertEqual(commit['evidence']['sourceState']['mode'], 'Ready')
                        self.assertIn('inferred', commit['evidence']['replayMethod'].lower())

    def test_copy_and_adjacent_navigation_follow_actual_ready_phases(self):
        wanted = {
            'historical-01279-keys': '11:57.033',
            'historical-01323-keys': '12:13.333',
            'historical-02549-keys': '22:02.400',
        }
        with tempfile.TemporaryDirectory(prefix='otis-bar33-neighbor-') as folder:
            output = Path(folder) / 'manifest.json'
            subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                            '--workspace', str(Path(folder) / 'work'), '--output', str(output),
                            '--include-intro'], check=True, capture_output=True)
            steps = json.loads(output.read_text())['steps']
            by_id = {step['id']: step for step in steps}
            for action_id, clock in wanted.items():
                with self.subTest(action=action_id):
                    self.assertEqual(by_id[action_id]['videoTime'], clock)
                    self.assertEqual(by_id[action_id]['action']['keys'], ['Ctrl+C'])
            # The source remains at the accepted origin; old reselect triplets
            # only compensated for the incorrect immediate Return movement.
            for action_id in ['historical-01276-keys', 'historical-01277-address',
                              'historical-01278-select', 'historical-01320-keys',
                              'historical-01321-address', 'historical-01322-select']:
                with self.subTest(retired=action_id):
                    self.assertFalse(action_id in by_id, "Redundant origin reselect remains: " + action_id)
            for address, clock in [('C17','13:19.667'), ('C19','13:20.000'),
                                   ('C21','13:22.867'), ('D41','22:21.900')]:
                with self.subTest(next=address):
                    candidates = [step for step in steps if step['videoTime'] == clock
                                  and step.get('expect', {}).get('nameBoxText') == address]
                    self.assertEqual(len(candidates), 1)
                    self.assertEqual(candidates[0]['action'],
                                     {'type':'key_sequence','target':'worksheet.grid','keys':['Down']})
            for commit_id, suffix_id in [('historical-01496-commit-local-pointed-formula','historical-01495-draft'),
                                         ('historical-01518-commit-local-pointed-formula','historical-01517-draft'),
                                         ('historical-03202-commit-local-pointed-formula','historical-03201-draft')]:
                with self.subTest(suffix=suffix_id):
                    self.assertLess(steps.index(by_id[suffix_id]), steps.index(by_id[commit_id]))

    def test_followup_copy_and_next_cells_keep_distinct_source_times(self):
        with tempfile.TemporaryDirectory(prefix='otis-bar33-followup-') as folder:
            output = Path(folder) / 'manifest.json'
            subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                            '--workspace', str(Path(folder) / 'work'), '--output', str(output),
                            '--include-intro'], check=True, capture_output=True)
            steps = json.loads(output.read_text())['steps']
            by_id = {step['id']: step for step in steps}
            for identifier, clock in [('historical-01418-keys','12:46.667'),
                                      ('historical-02162-keys','18:26.067'),
                                      ('historical-02163-address','18:26.067'),
                                      ('historical-02164-select','18:26.067'),
                                      ('historical-02165-draft','18:26.433'),
                                      ('historical-04572-source-insert','22:15.533'),
                                      ('historical-04573-source-insert','22:15.533'),
                                      ('historical-04574-source-insert','22:15.533')]:
                with self.subTest(phase=identifier):
                    self.assertEqual(by_id[identifier]['videoTime'], clock)
            for identifier in ['historical-01415-keys','historical-01416-address','historical-01417-select']:
                with self.subTest(retired=identifier):
                    self.assertFalse(identifier in by_id, "Redundant origin reselect remains: " + identifier)

    def test_dense_neighbors_preserve_every_distinct_inspection_and_source_observation(self):
        plan = json.loads((REVIEW.parent / 'planned-neighbor-contract.json').read_text())
        for relative, expected_sha in plan['sourceProofs'].items():
            self.assertEqual(hashlib.sha256((EVIDENCE / relative).read_bytes()).hexdigest(), expected_sha)
        with tempfile.TemporaryDirectory(prefix='otis-bar33-dense-neighbors-') as folder:
            output = Path(folder) / 'manifest.json'
            subprocess.run([sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
                            '--workspace', str(Path(folder) / 'work'), '--output', str(output),
                            '--include-intro'], check=True, capture_output=True)
            steps = json.loads(output.read_text())['steps']
            by_id = {step['id']: step for step in steps}
            for phase in plan['retime']:
                with self.subTest(clock=phase['id']):
                    seconds=phase['seconds']
                    self.assertEqual(by_id[phase['id']]['videoTime'], f'{int(seconds//60):02d}:{seconds%60:06.3f}')
            for phase in plan['remove']:
                with self.subTest(retired=phase['id']):
                    self.assertFalse(phase['id'] in by_id, 'Redundant coarse selection remains: '+phase['id'])
            for phase in plan['new']:
                with self.subTest(distinct=phase['id']):
                    actual=by_id.get(phase['id']);self.assertIsNotNone(actual)
                    expected_action=dict(phase['action'])
                    if phase.get('held'):expected_action['releaseModifiers']=False
                    self.assertEqual(actual['action'],expected_action)
                    seconds=phase['seconds']
                    self.assertEqual(actual['videoTime'],f'{int(seconds//60):02d}:{seconds%60:06.3f}')
                    for key,value in phase.get('expect',{}).items():self.assertEqual(actual['expect'].get(key),value)
            with self.subTest(additional_commit='C72'):
                accepted=by_id['historical-03988-commit-autosum']
                self.assertEqual(accepted['action']['keys'], ['Ctrl+Return'])
                self.assertEqual(accepted['expect']['nameBoxText'],'C72')
            original = {step['id']: step for step in json.loads((REVIEW.parent / 'initial-green-v1/scope-include-intro-before.json').read_text())['steps']}
            for actual in steps:
                if actual.get('evidence',{}).get('phaseLedger')=='historical-bar-commit-header-phases.json' and actual['id'] in original:
                    with self.subTest(observation=actual['id']):
                        self.assertEqual(actual['evidence']['eventId'],original[actual['id']]['evidence']['eventId'])


if __name__ == '__main__':
    unittest.main()
