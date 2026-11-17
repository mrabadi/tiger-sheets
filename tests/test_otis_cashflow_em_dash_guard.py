"""The read-only source guard preserves an imported literal em dash."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'
PROOF=BASE/'post-attempt11-cashflow-em-dash-guard'


def restore_reviewed_c37_guard_records(verifier, records, mode):
    """Restore precisely one pinned expectation before older invariance checks."""
    path=PROOF/'exact-generated-action-delta.json'
    verifier.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                         '2716ffd29f23fb2531e10e241a635a69a560d197fcb5efb056c7c2811d4165a0')
    delta=json.loads(path.read_text())[mode]
    identifier={'include-intro':'historical-04731-source-insert',
                'default':'historical-04136-source-insert'}[mode]
    verifier.assertEqual(delta['addedRecords'],[])
    verifier.assertEqual(delta['removedRecords'],[])
    verifier.assertEqual(delta['beforeCount'],delta['afterCount'])
    verifier.assertEqual([r['id'] for r in delta['changedRecords']],[identifier])
    change=delta['changedRecords'][0]
    before_path=ROOT/delta['before']['path']
    verifier.assertEqual(hashlib.sha256(before_path.read_bytes()).hexdigest(),delta['before']['sha256'])
    prior=[r for r in json.loads(before_path.read_text())['steps'] if r['id']==identifier]
    verifier.assertEqual(prior,[change['before']])
    verifier.assertEqual(change['before']['expect']['cells'],[{'address':'C37','input':'-'}])
    expected=copy.deepcopy(change['before'])
    expected['expect']['cells']=[{'address':'C37','input':"'—",'display':'—','style':{'quotePrefix':False}}]
    verifier.assertEqual(change['after'],expected)
    matches=[i for i,r in enumerate(records) if r['id']==identifier]
    verifier.assertEqual(len(matches),1)
    verifier.assertEqual(records[matches[0]],change['after'],
                         'Only the exact reviewed C37 guard after-image may be restored')
    restored=copy.deepcopy(records)
    restored[matches[0]]=copy.deepcopy(change['before'])
    return restored


class CashflowEmDashGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory()
        cls.generated={}
        cls.workspaces={}
        for mode in ['include-intro','default']:
            output=Path(cls.folder.name)/(mode+'.json')
            command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                     '--workspace',str(Path(cls.folder.name)/(mode+'-work')),'--output',str(output)]
            if mode=='include-intro':command.append('--include-intro')
            subprocess.run(command,check=True,capture_output=True)
            manifest=json.loads(output.read_text())
            cls.generated[mode]=manifest['steps']
            cls.workspaces[mode]=manifest['workspace']

    @classmethod
    def tearDownClass(cls):cls.folder.cleanup()

    def assert_literal_guard(self,record):
        self.assertEqual(record['expect']['cells'],[
            {'address':'C37','input':"'—",'display':'—','style':{'quotePrefix':False}}])

    def test_guard_checks_actual_internal_literal_and_visible_em_dash(self):
        for mode,records in self.generated.items():
            with self.subTest(mode=mode):
                row=next(r for r in records if r['evidence'].get('eventId')=='history-1251'
                         and r['action'].get('keys')==[])
                self.assert_literal_guard(row)
                self.assertEqual(row['action'],{'type':'key_sequence','target':'worksheet.grid','keys':[]})

    def test_original_year_column_and_retained_prestate_are_text_u2014(self):
        sys.path.insert(0,'/tmp/tigersheets-biff-reference')
        import xlrd
        official=xlrd.open_workbook(str(BASE/'reference/101-01-Otis-10-K-Raw.xls')).sheet_by_name('cash flows')
        retained=xlrd.open_workbook(str(PROOF/'retained-prestate/Otis-10-K-Raw.xls')).sheet_by_name('cash flows')
        self.assertEqual((official.cell_type(36,18),official.cell_value(36,18)),(xlrd.XL_CELL_TEXT,'—'))
        self.assertEqual((retained.cell_type(36,2),retained.cell_value(36,2)),(xlrd.XL_CELL_TEXT,'—'))
        self.assertEqual([ord(c) for c in retained.cell_value(36,2)],[0x2014])

    def test_only_the_guard_changes_and_ascii_or_zero_forgery_is_rejected(self):
        for mode,records in self.generated.items():
            from test_otis_live_selection_phases import ModalHeldSelectionTests
            reviewed=ModalHeldSelectionTests.restore_reviewed_post12_source_records(
                self,records,mode,self.workspaces[mode])
            before=json.loads((PROOF/'before'/(mode+'-generated-history.json')).read_text())['steps']
            def normalize(rows):
                rows=copy.deepcopy(rows)
                def visit(rs):
                    for r in rs:
                        if r['action'].get('target')=='fileNameEdit' and r['action'].get('type')=='text':
                            r['action']['text']=Path(r['action']['text']).name
                        visit(r['action'].get('steps',[]))
                visit(rows);return rows
            old,new=normalize(before),normalize(reviewed)
            self.assertEqual([r['id'] for r in new],[r['id'] for r in old])
            changes=[(a,b) for a,b in zip(old,new) if a!=b]
            self.assertEqual(len(changes),1,'Only the read-only C37 expectation may change')
            a,b=changes[0];expected=copy.deepcopy(a);expected['expect']['cells']=[
                {'address':'C37','input':"'—",'display':'—','style':{'quotePrefix':False}}]
            self.assertEqual(b,expected,'Actions, timing, evidence, IDs and order remain exact')
            for forged in ['-','0']:
                bad=copy.deepcopy(b);bad['expect']['cells'][0]['input']=forged
                with self.assertRaises(AssertionError):self.assert_literal_guard(bad)
                damaged=copy.deepcopy(reviewed)
                damaged[next(i for i,r in enumerate(damaged) if r['id']==b['id'])]=bad
                with self.assertRaisesRegex(AssertionError,'exact reviewed C37 guard after-image'):
                    restore_reviewed_c37_guard_records(self,damaged,mode)
            restored=restore_reviewed_c37_guard_records(self,reviewed,mode)
            self.assertEqual(normalize(restored),old)


if __name__=='__main__':unittest.main()
