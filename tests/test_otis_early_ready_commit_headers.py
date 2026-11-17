"""Reviewed first Ready headers must survive independently of later navigation."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
E=ROOT/'docs/validation/otis-90-minute'


class EarlyReadyCommitHeaders(unittest.TestCase):
    def test_reviewed_formula_bar_commits_keep_the_source_ready_origins(self):
        with tempfile.TemporaryDirectory(prefix='otis-bar-ready-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),'--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                subprocess.run(command,check=True,capture_output=True)
                steps=json.loads(output.read_text())['steps']
                candidates=[s for s in steps if s['action'].get('target')=='formula.input'
                    and any(c.get('address')=='D36' and c.get('input')=="='[Otis-10-K-Raw.xls]consolidated balance sheets'!B23" for c in s.get('expect',{}).get('cells',[]))]
                self.assertEqual(len(candidates),1)
                commit=candidates[0]
                self.assertEqual(commit['action']['keys'],['Ctrl+Return'])
                self.assertEqual(commit['videoTime'],'22:02.267')
                self.assertEqual({k:commit['expect'].get(k) for k in ['row','column','nameBoxText']},
                    {'row':36,'column':4,'nameBoxText':'D36'})
                copied=[s for s in steps if s['action'].get('keys')==['Ctrl+C'] and s['videoTime']=='22:02.400']
                self.assertEqual(len(copied),1)
                self.assertLess(steps.index(commit),steps.index(copied[0]))

    def test_adjacent_reviewed_dimensions_are_actual_held_phases(self):
        expected=[('d2-2c','08:48.033','1R x 2C'),('c6-2c','11:59.600','1R x 2C'),
            ('c11-2c','12:25.533','1R x 2C'),('c11-3c','12:25.733','1R x 3C'),
            ('b42-2c','21:13.133','1R x 2C'),('b42-3c','21:13.267','1R x 3C'),
            ('b59-2c','29:54.267','1R x 2C'),('b59-2r4c','29:54.300','2R x 4C'),
            ('b59-2r5c','29:54.333','2R x 5C'),('b59-2r7c','29:54.367','2R x 7C'),
            ('b59-2r8c','29:54.467','2R x 8C'),('b59-2r9c','29:54.633','2R x 9C'),
            ('b59-1r9c','29:54.800','1R x 9C'),
            ('c62-2r','31:23.433','2R x 1C'),('c62-3r','31:23.600','3R x 1C'),
            ('c62-3r2c','31:23.800','3R x 2C'),('c62-3r3c','31:23.967','3R x 3C'),
            ('b44-2r','21:47.833','2R x 1C'),
            ('d44-2r','23:22.767','2R x 1C'),('d44-3r','23:22.933','3R x 1C'),
            ('d44-3r2c','23:23.467','3R x 2C'),('b62-2r','29:05.500','2R x 1C'),
            ('b36-4r','21:14.633','4R x 1C'),('b36-5r','21:14.667','5R x 1C'),
            ('b36-6r','21:15.067','6R x 1C'),
            ('b33-2r3c','21:16.933','2R x 3C'),('b33-2r4c','21:16.967','2R x 4C'),
            ('b33-2r6c','21:17.033','2R x 6C'),('b33-1r7c','21:17.067','1R x 7C'),
            ('b33-1r8c','21:17.167','1R x 8C'),('b33-2r8c','21:17.233','2R x 8C'),
            ('b33-2r9c','21:17.267','2R x 9C'),('b33-1r9c','21:17.500','1R x 9C')]
        with tempfile.TemporaryDirectory(prefix='otis-early-counts-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),'--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                subprocess.run(command,check=True,capture_output=True)
                steps=json.loads(output.read_text())['steps'];byid={s['id']:s for s in steps}
                for suffix,time,header in expected:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-early-held-'+suffix
                        self.assertIn(identifier,byid)
                        phase=byid[identifier]
                        self.assertEqual((phase['videoTime'],phase['expect']['nameBoxText']),(time,header))
                        action=phase['action']
                        self.assertTrue((action['type']=='mouse_cell' and action['gesture']=='move')
                            or (action['type']=='key_sequence' and action.get('releaseModifiers') is False))
                for prefix in ['08:48.','12:25.','21:13.','29:54.','31:23.']:
                    with self.subTest(mode=mode,range=prefix):
                        self.assertFalse(any(s['videoTime'].startswith(prefix)
                            and s['action'].get('target')=='formula.name' and ':' in s['action'].get('text','') for s in steps))
        source={r['id']:r for r in json.loads((E/'historical-statements-visual-steps.json').read_text())['observations']}
        for identifier,required in [('history-0702','B42:D42'),('history-0704','B36:B41'),('history-0825','D44:E46')]:
            self.assertIn(required,source[identifier]['observation'])
        self.assertNotIn('B42:E42',source['history-0702']['observation'])
        self.assertNotIn('B36:B40',source['history-0704']['observation'])
        self.assertNotIn('D44:F46',source['history-0825']['observation'])

    def test_all_seventy_reviewed_commits_retain_the_coherent_source_target(self):
        report_path=E/'post-attempt12-early70-commit-trajectory-review/review.json'
        self.assertEqual(hashlib.sha256(report_path.read_bytes()).hexdigest(),'37948f9b08111c5aebc7f4096dadbdffddbf743ca3854d9f2ced0eccb008259f')
        review=json.loads(report_path.read_text())
        mapping=json.loads((E/'post-attempt12-source-zero-commit-review/source-refinement/early70-mode-action-map.json').read_text())
        mapped={r['sourceReviewId']:r for r in mapping['rows']}
        self.assertEqual(len(review['rows']),70)
        self.assertEqual(set(mapped),{r['currentId'] for r in review['rows']})
        with tempfile.TemporaryDirectory(prefix='otis-early-ready-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),'--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                subprocess.run(command,check=True,capture_output=True)
                byid={s['id']:s for s in json.loads(output.read_text())['steps']}
                baseline=json.loads((E/'post-attempt12-source-zero-commit-review/source-refinement/before'/f'{mode}-generated-history.json').read_text())['steps']
                for row in review['rows']:
                    with self.subTest(mode=mode,source=row['currentId']):
                        special=row['currentId']=='historical-02632-commit-reference'
                        identifier='historical-early-d41-initial-commit' if special else mapped[row['currentId']][mode]
                        self.assertTrue(identifier in byid,identifier)
                        record=byid[identifier]
                        self.assertEqual(record['action'],{'type':'key_sequence','target':'formula.input' if special else 'worksheet.cellEditor','keys':['Ctrl+Return']})
                        ready=row['firstCoherentReadyTarget'];seconds=ready['seconds']
                        self.assertEqual(record['videoTime'],f'{int(seconds//60):02d}:{seconds%60:06.3f}')
                        target=row['target'];column=0
                        for char in target.rstrip('0123456789'):column=column*26+ord(char)-64
                        self.assertEqual({k:record['expect'][k] for k in ['row','column','nameBoxText']},
                            {'row':int(''.join(c for c in target if c.isdigit())),'column':column,'nameBoxText':target})
                        self.assertEqual(record['evidence']['sourceState']['mode'],'Ready')
                        self.assertIn('inferred',record['evidence']['replayMethod'].lower())
                        if row['currentId'].endswith('-commit-autosum'):
                            commit_id=mapped[row['currentId']][mode]
                            before_index=next(i for i,s in enumerate(baseline) if s['id']==commit_id)
                            actual_opener=next(s for s in reversed(baseline[:before_index])
                                if s['action'].get('keys')==['Alt+='])
                            opener=byid[actual_opener['id']]
                            self.assertLess(list(byid).index(opener['id']),list(byid).index(identifier),
                                'Actual AutoSum must open before the dense retained Ready commit')
                        for cell in row['currentRecord'].get('expect',{}).get('cells',[]):
                            actual=next(c for c in record['expect']['cells'] if c['address']==cell['address'])
                            for key,value in cell.items():self.assertEqual(actual[key],value)
                for identifier,time,action in [
                    ('historical-early-b11-interim-commit','10:30.433',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Ctrl+Return']}),
                    ('historical-early-b11-inspection-edit','10:30.600',{'type':'key_sequence','target':'worksheet.grid','keys':['F2']})]:
                    with self.subTest(mode=mode,special=identifier):
                        self.assertTrue(identifier in byid,identifier)
                        self.assertEqual((byid[identifier]['videoTime'],byid[identifier]['action']),(time,action))
                inspection=byid[mapped['historical-02632-commit-reference'][mode]]
                self.assertEqual(inspection['videoTime'],'22:36.933')
                self.assertEqual(inspection['expect']['nameBoxText'],'D41')


if __name__=='__main__':unittest.main()
