"""The observed B59 draft commits in place before its first held header."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/validation/otis-90-minute'


class B59CommitRetention(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.generated = {}
        cls.workspaces = {}
        for mode in ['include-intro', 'default']:
            path = Path(cls.folder.name) / (mode+'.json')
            command = [sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                       '--workspace',str(Path(cls.folder.name)/(mode+'-work')),'--output',str(path)]
            if mode=='include-intro': command.append('--include-intro')
            subprocess.run(command,check=True,capture_output=True)
            manifest=json.loads(path.read_text())
            cls.generated[mode] = manifest['steps']
            cls.workspaces[mode] = manifest['workspace']

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_source_ready_commits_in_place_after_the_last_enter_frame(self):
        for mode, steps in self.generated.items():
            with self.subTest(mode=mode):
                commit = next(s for s in steps if s['id'].endswith('-commit')
                              and any(c.get('address')=='B59' for c in s.get('expect',{}).get('cells',[])))
                self.assertEqual(commit['videoTime'],'28:46.033')
                self.assertEqual(commit['action'],{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Ctrl+Return']})
                self.assertEqual({k:commit['expect'][k] for k in ['row','column','nameBoxText']},
                                 {'row':59,'column':2,'nameBoxText':'B59'})
                self.assertIn('inferred',commit['evidence']['replayMethod'].lower())
                self.assertTrue(Path(commit['evidence']['sourceFrame']).is_file())

    def test_coarse_commit_frame_remains_an_uncommitted_draft_checkpoint(self):
        for mode, steps in self.generated.items():
            with self.subTest(mode=mode):
                drafts = [s for s in steps if s['videoTime']=='28:45.500'
                          and s.get('expect',{}).get('formulaDraft')=='Cash Flow from Operations:']
                self.assertEqual(len(drafts),1,'Original 28:45.5 is still Enter, not a completed value')
                draft=drafts[0]
                self.assertEqual(draft['action'],{'type':'key_sequence','target':'worksheet.cellEditor','keys':[]})
                self.assertEqual(draft['expect']['cells'],[{'address':'B59','input':''}])
                self.assertEqual((draft['expect']['row'],draft['expect']['column'],draft['expect']['nameBoxText']),(59,2,'B59'))
                self.assertFalse(any(s['action'].get('keys')==['Return'] and s['action'].get('target')=='worksheet.cellEditor'
                                     for s in steps if '28:45.000'<=s['videoTime']<'28:46.467'))

    def test_every_other_existing_action_and_its_order_is_preserved(self):
        before=BASE/'post-attempt10-b59-commit-retention/before'
        for mode,steps in self.generated.items():
            old=json.loads((before/(mode+'-generated-history.json')).read_text())['steps']
            from test_otis_live_selection_phases import ModalHeldSelectionTests
            reviewed=ModalHeldSelectionTests.restore_reviewed_post12_source_records(
                self,steps,mode,self.workspaces[mode])
            old_by={s['id']:s for s in old};now_by={s['id']:s for s in reviewed}
            self.assertTrue(set(old_by)<=set(now_by),'No old source action ID is retired')
            allowed={'history-1085','history-1086'}
            # Generated Raw Open paths belong to their independent workspace.
            def normalize(records):
                values=copy.deepcopy([s for s in records if s['evidence'].get('eventId') not in allowed])
                def walk(rows):
                    for row in rows:
                        a=row['action']
                        if a.get('target')=='fileNameEdit' and a.get('type')=='text':
                            a['text']=Path(a['text']).name
                        walk(a.get('steps',[]))
                walk(values);return values
            expected=normalize(old)
            from test_otis_cashflow_em_dash_guard import restore_reviewed_c37_guard_records
            self.assertEqual(normalize(restore_reviewed_c37_guard_records(self,reviewed,mode)),expected)
            damaged=copy.deepcopy(steps)
            row=next(s for s in damaged if s['evidence'].get('eventId') not in allowed)
            row['action']={'type':'key_sequence','target':'worksheet.grid','keys':['Escape']}
            with self.assertRaisesRegex(AssertionError,'New source routes must equal their exact reviewed after-image'):
                ModalHeldSelectionTests.restore_reviewed_post12_source_records(
                    self,damaged,mode,self.workspaces[mode])
        # The existing layered digest must reject wrong B59 after-images too,
        # rather than merely excluding the two reviewed source observations.
        from test_otis_live_selection_phases import ModalHeldSelectionTests
        verifier=ModalHeldSelectionTests()
        damaged=copy.deepcopy(self.generated['default'])
        next(s for s in damaged if s['id']=='historical-02828-commit')['action']['keys']=['Return']
        with self.assertRaisesRegex(AssertionError,'exact reviewed B59'):
            verifier.restore_reviewed_b59_commit_records(damaged)
        damaged=copy.deepcopy(self.generated['default'])
        next(s for s in damaged if s['id']=='historical-text-checkpoint-b59-enter-1725500')['expect']['cells'][0]['input']='Cash Flow from Operations:'
        with self.assertRaisesRegex(AssertionError,'exact reviewed after-image'):
            verifier.restore_reviewed_b59_commit_records(damaged)


if __name__=='__main__':
    unittest.main()
