"""Source-visible zero drafts commit to the observed Ready cell, not below it."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'


class CashflowZeroPhases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory(prefix='otis-zero-phases-')
        cls.generated={}
        for mode in ['include-intro','default']:
            output=Path(cls.folder.name)/(mode+'.json')
            command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                '--workspace',str(Path(cls.folder.name)/(mode+'-work')),'--output',str(output)]
            if mode=='include-intro':command.append('--include-intro')
            subprocess.run(command,check=True,capture_output=True)
            cls.generated[mode]=json.loads(output.read_text())['steps']

    @classmethod
    def tearDownClass(cls):cls.folder.cleanup()

    def test_first_two_rows_preserve_the_actual_ready_header_trajectory(self):
        expected=[('C37','32:22.333','32:22.500',['Tab'],'D37',37,4),
            ('D37','32:22.633','32:22.867',['Ctrl+Return'],'D37',37,4),
            ('C38','32:23.533','32:23.667',['Tab'],'D38',38,4),
            ('D38','32:23.800','32:24.000',['Ctrl+Return'],'D38',38,4)]
        for mode,steps in self.generated.items():
            for address,draft_time,commit_time,keys,ready,row,column in expected:
                with self.subTest(mode=mode,address=address):
                    commits=[s for s in steps if s['id'].endswith('-commit')
                        and any(c.get('address')==address and c.get('input')=='0' for c in s.get('expect',{}).get('cells',[]))
                        and s['evidence'].get('eventId') in ['history-1252','history-1254']]
                    self.assertEqual(len(commits),1)
                    commit=commits[0]
                    self.assertEqual(commit['videoTime'],commit_time)
                    self.assertEqual(commit['action'],{'type':'key_sequence','target':'worksheet.cellEditor','keys':keys})
                    self.assertEqual({k:commit['expect'][k] for k in ['nameBoxText','row','column']},
                        {'nameBoxText':ready,'row':row,'column':column})
                    self.assertEqual(commit['evidence']['sourceState']['mode'],'Ready')
                    self.assertIn('inferred',commit['evidence']['replayMethod'].lower())
                    preceding=next(s for s in reversed(steps[:steps.index(commit)]) if s['action'].get('type')=='text' and s['action'].get('target')=='worksheet.grid')
                    self.assertEqual((preceding['videoTime'],preceding['action']['text']),(draft_time,'0'))
            for identifier,time,address,row in [
                ('historical-zero-return-c37','32:23.033','C37',37),
                ('historical-zero-return-c38','32:24.200','C38',38)]:
                with self.subTest(mode=mode,navigation=address):
                    stops=[s for s in steps if s['id']==identifier]
                    self.assertEqual(len(stops),1,'Observed return-to-first-column header must be retained')
                    stop=stops[0]
                    self.assertEqual(stop['videoTime'],time)
                    self.assertEqual(stop['action'],{'type':'key_sequence','target':'worksheet.grid','keys':['Left']})
                    self.assertEqual({k:stop['expect'][k] for k in ['nameBoxText','row','column']},
                        {'nameBoxText':address,'row':row,'column':3})
                    self.assertEqual(stop['evidence']['sourceState']['mode'],'Ready')

    def test_remaining_zero_entries_keep_the_observed_ready_cell(self):
        expected=[('C39','32:25.133','32:25.233'),('C40','32:25.800','32:25.967'),
            ('C41','32:26.500','32:26.633'),('E41','32:27.367','32:27.533'),
            ('C43','32:28.667','32:28.833'),('C44','32:29.467','32:29.633'),
            ('D44','32:30.233','32:30.467'),('E46','32:32.867','32:33.367'),
            ('C30','32:35.167','32:35.333'),('E12','34:09.133','34:09.367')]
        for mode,steps in self.generated.items():
            for address,draft_time,commit_time in expected:
                with self.subTest(mode=mode,address=address):
                    matches=[s for s in steps if s['id'].endswith('-commit')
                        and any(c.get('address')==address and c.get('input')=='0' for c in s.get('expect',{}).get('cells',[]))
                        and s['evidence'].get('eventId','')>='history-1256']
                    self.assertEqual(len(matches),1)
                    commit=matches[0]
                    self.assertEqual(commit['videoTime'],commit_time)
                    self.assertEqual(commit['action'],{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Ctrl+Return']})
                    self.assertEqual(commit['expect']['nameBoxText'],address)
                    self.assertEqual(commit['expect']['row'],int(address[1:]))
                    self.assertEqual(commit['expect']['column'],ord(address[0])-64)
                    cell=next(c for c in commit['expect']['cells'] if c['address']==address)
                    self.assertEqual((cell['value'],cell['valueType']),(0,'number'))
                    draft=next(s for s in reversed(steps[:steps.index(commit)]) if s['action'].get('type')=='text' and s['action'].get('target')=='worksheet.grid')
                    self.assertEqual((draft['videoTime'],draft['action']['text']),(draft_time,'0'))

    def test_adjacent_navigation_cancel_and_save_are_actual_source_phases(self):
        stops=[('historical-zero-return-d41','32:27.733','D41',['Left']),
            ('historical-zero-return-c41','32:27.900','C41',['Left']),
            ('historical-zero-return-c42','32:28.133','C42',['Down']),
            ('historical-zero-return-d46','32:31.133','D46',['Down']),
            ('historical-zero-return-d47','32:31.300','D47',['Down'])]
        for mode,steps in self.generated.items():
            by_id={s['id']:s for s in steps}
            for identifier,time,address,keys in stops:
                with self.subTest(mode=mode,navigation=identifier):
                    self.assertTrue(identifier in by_id,identifier)
                    record=by_id[identifier]
                    self.assertEqual(record['videoTime'],time)
                    self.assertEqual(record['action'],{'type':'key_sequence','target':'worksheet.grid','keys':keys})
                    self.assertEqual(record['expect']['nameBoxText'],address)
            with self.subTest(mode=mode,phase='cancelled-zero'):
                self.assertTrue('historical-zero-e47-cancelled-draft' in by_id,'Missing actual E47 zero draft')
                draft=by_id['historical-zero-e47-cancelled-draft']
                self.assertEqual((draft['videoTime'],draft['action'],draft['expect']['formulaDraft']),
                    ('32:31.733',{'type':'text','target':'worksheet.grid','text':'0'},'0'))
                self.assertTrue('historical-zero-e47-cancel' in by_id,'Missing actual E47 cancellation')
                cancel=by_id['historical-zero-e47-cancel']
                self.assertEqual((cancel['videoTime'],cancel['action']),
                    ('32:32.233',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Escape']}))
                self.assertEqual(cancel['expect']['nameBoxText'],'E47')
                self.assertEqual(cancel['expect']['cells'],[{'address':'E47','input':'-19','value':-19,'valueType':'number'}])
            with self.subTest(mode=mode,phase='source-save'):
                self.assertTrue('historical-zero-raw-save' in by_id,'Missing actual Raw Save boundary')
                save=by_id['historical-zero-raw-save']
                self.assertEqual((save['videoTime'],save['action']),
                    ('32:35.733',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+S']}))
                self.assertIn('inferred',save['evidence']['replayMethod'].lower())


if __name__=='__main__':unittest.main()
