"""Original early count headers are replayed as actual held selections."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class RemainingEarlyRangeHeaders(unittest.TestCase):
    def test_bar_neighbor_boundaries_keep_active_cells_and_distinct_inspections(self):
        with tempfile.TemporaryDirectory(prefix='otis-bar-neighbor-followup-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                steps=json.loads(output.read_text())['steps']
                def flatten(records):
                    for record in records:
                        yield record
                        yield from flatten(record['action'].get('steps',[]))
                all_records=list(flatten(steps));byid={s['id']:s for s in all_records}
                phases=[('c4-active-c5','11:58.333','C5',['Tab']*3),
                    ('c14-active-c15','12:47.833','C15',['Tab']),
                    ('c14-copy-end','12:47.967','C15',['Escape']),
                    ('f2-inspection-second-open','26:42.767','SUM',['F2']),
                    ('f2-inspection-second-close','26:42.867','F2',['Escape'])]
                for suffix,time,header,keys in phases:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        record=byid[identifier]
                        self.assertEqual((record['videoTime'],record['expect']['nameBoxText'],record['action']['keys']),(time,header,keys))
                groups={s.get('evidence',{}).get('phaseGroup'):s for s in all_records}
                c30=[s for s in all_records if s.get('evidence',{}).get('phaseGroup')=='remaining-early-neighbor-c30-inspection']
                c30_f2=[s for s in c30 if s['action']=={'type':'key_sequence','target':'worksheet.grid','keys':['F2']}]
                self.assertEqual(len(c30_f2),1,'The C30 group has one genuine inline inspection opener')
                self.assertEqual([s['id'] for s in c30],{
                    'include-intro':['historical-02214-keys','historical-02215-keys','historical-02216-keys','historical-02219-draft'],
                    'default':['historical-01621-keys','historical-01622-keys','historical-01623-keys','historical-01626-draft']}[mode])
                self.assertEqual([s['videoTime'] for s in c30],['18:50.433','18:51.367','18:51.733','18:52.267'])
                self.assertEqual([s['action'] for s in c30],[
                    {'type':'key_sequence','target':'worksheet.grid','keys':['F2']},
                    {'type':'key_sequence','target':'worksheet.cellEditor','keys':['Escape']},
                    {'type':'key_sequence','target':'worksheet.grid','keys':['Down']},
                    {'type':'text','target':'worksheet.grid','text':'='}])
                formula="='[Otis-10-K-Raw.xls]consolidated balance sheets'!B15+'[Otis-10-K-Raw.xls]consolidated balance sheets'!B17+'[Otis-10-K-Raw.xls]consolidated balance sheets'!B18"
                cells=[{'address':'C29','value':1546,'valueType':'number','display':'1,546','style':{'numberFormat':'#,##0'}},
                    {'address':'C27','value':1799,'valueType':'number','display':'1799'},
                    {'address':'C30','input':formula,'value':3031,'valueType':'number'}]
                self.assertEqual(c30[0]['expect'],{'row':30,'column':3,'nameBoxText':'SUM','formulaDraft':formula,'cells':cells})
                self.assertEqual(c30[1]['expect'],{'row':30,'column':3,'nameBoxText':'C30','formulaDraft':formula,'cells':cells})
                self.assertEqual(c30[2]['expect'],{'row':31,'column':3,'nameBoxText':'C31','formulaDraft':''})
                self.assertEqual(c30[3]['expect'],{'row':31,'column':3,'formulaDraft':'='},
                    'The source SUM versus native C31 fresh-draft header remains a disclosed gap, not a false matching assertion')
                groups['remaining-early-neighbor-c30-inspection']=c30_f2[0]
                for group,time in [('remaining-early-neighbor-c8-first-held','12:13.600'),
                    ('remaining-early-neighbor-c30-inspection','18:50.433'),
                    ('remaining-early-neighbor-c33-draft','19:04.400'),
                    ('remaining-early-neighbor-d41-draft','22:22.533'),
                    ('remaining-early-neighbor-c63-draft','31:10.767'),
                    ('remaining-early-neighbor-c70-draft','32:48.933')]:
                    with self.subTest(mode=mode,group=group):
                        self.assertTrue(group in groups,group)
                        self.assertEqual(groups[group]['videoTime'],time)

    def test_copied_d36_range_reaches_find_with_d37_active_without_collapsing(self):
        with tempfile.TemporaryDirectory(prefix='otis-d36-tab-source-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                steps=json.loads(output.read_text())['steps'];byid={s['id']:s for s in steps}
                expected=[('d36-2r','22:02.667','2R x 1C',36),
                    ('d36-3r','22:02.867','3R x 1C',36),
                    ('d36-release','22:03.633','D36',36),
                    ('d36-tab-active','22:03.633','D37',37)]
                for suffix,time,header,row in expected:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        actual=byid[identifier]
                        self.assertEqual((actual['videoTime'],actual['expect']['nameBoxText'],
                            actual['expect']['row'],actual['expect']['column']),(time,header,row,4))
                if 'historical-remaining-early-d36-tab-active' not in byid:continue
                tab=byid['historical-remaining-early-d36-tab-active']
                self.assertEqual(tab['action'],{'type':'key_sequence','target':'worksheet.grid','keys':['Tab']})
                self.assertEqual(tab['evidence']['sourceState']['retainedSelectionRange'],'D36:D38')
                find=[s for s in steps if s['evidence'].get('eventId')=='history-0748'
                    and s['action'].get('keys')==['Ctrl+F']]
                self.assertEqual(len(find),1)
                self.assertEqual((find[0]['videoTime'],find[0]['expect']['nameBoxText']),('22:03.633','D37'))
                self.assertEqual(find[0]['evidence']['sourceState']['retainedSelectionRange'],'D36:D38')
                self.assertLess(steps.index(tab),steps.index(find[0]))
                self.assertFalse(any(s['action'].get('text')=='D36:D38' for s in steps))

    def test_dense_source_interpretations_retire_unobserved_reselections(self):
        observations={r['id']:r for r in json.loads((ROOT/'docs/validation/otis-90-minute/historical-statements-visual-steps.json').read_text())['observations']}
        contracts={'history-0654':'Held A29:A31 is 3R x 1C; release retains that three-row range',
            'history-0655':'Released A29 still anchors the prior three-row range',
            'history-0657':'NameBox C32 and formula606 establish the lower-right active cell',
            'history-0734':'B42 drag briefly shows 2R x 5C before returning to one row',
            'history-0964':'NameBox A31 and white A31 anchor the reverse A30:A31 range',
            'history-1125':'NameBox B66 and Cash Flow from Financing identify the actual target'}
        for event,prefix in contracts.items():
            with self.subTest(event=event):self.assertTrue(observations[event]['observation'].startswith(prefix),
                'Dense original header/body evidence must replace the unsupported old range interpretation')
        before=json.loads((ROOT/'docs/validation/otis-90-minute/post-attempt12-source-zero-commit-review/source-refinement/before/include-intro-generated-history.json').read_text())
        prior={r['id']:r for r in before['steps']}
        preserved={'history-'+n for n in ['0076','0093','0188','0244','0529','0703','0708','0779','0781','1068','1213']}
        ready={'entries':[]}
        for name in ['historical-early-ready-header-phases.json','historical-early-held-selection-phases.json']:
            ready['entries']+=json.loads((ROOT/'docs/validation/otis-90-minute'/name).read_text())['entries']
        seen=set()
        for group in ready['entries']:
            for update in group['updates']:
                identifier=update.get('match',{}).get('recordIdByMode',{}).get('include-intro')
                event=prior.get(identifier,{}).get('evidence',{}).get('eventId')
                if event in preserved:
                    seen.add(event)
                    with self.subTest(retimedOriginalObservation=event):
                        self.assertEqual(update['sourceEventId'],event,'Retimed existing observations retain their original source identity')
                        self.assertIn(event,group['sourceEventIds'])
        self.assertEqual(seen,preserved)

    def test_followup_frames_keep_local_fill_whole_columns_and_discrete_targets(self):
        expected=[('c11local-3c','13:13.867','1R x 3C'),('c11local-release','13:14.167','C11'),
            ('datecopy-3c','15:40.967','1R x 3C'),
            ('whole971-6c','16:11.000','6C'),('whole971-release','16:11.033','D5'),
            ('whole1030-6c','17:10.000','6C'),('whole1030-release','17:10.067','A9'),
            ('g17-release','14:35.567','I17'),('a31-reverse-range','25:31.000','A31')]
        with tempfile.TemporaryDirectory(prefix='otis-remaining-early-followup-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                steps=json.loads(output.read_text())['steps'];byid={s['id']:s for s in steps}
                for suffix,time,header in expected:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        self.assertEqual((byid[identifier]['videoTime'],byid[identifier]['expect']['nameBoxText']),(time,header))
                if 'historical-remaining-early-whole1030-6c' in byid:
                    self.assertEqual(byid['historical-remaining-early-whole1030-6c']['action'],
                        {'type':'key_sequence','target':'worksheet.grid','keys':['Shift+Right']*3,'releaseModifiers':False})
                    preparation=byid.get('historical-remaining-early-whole1030-column-span')
                    self.assertIsNotNone(preparation,'Actual imported A9:C9 must select its real merged span before inferred extension')
                    self.assertEqual(preparation['action'],{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+Space']})
                    self.assertNotIn('expect',preparation,'The native three-column preparation is not a source three-column observation')
                self.assertTrue(any(s['action']=={'type':'text','target':'formula.name','text':'B66'}
                    and s['videoTime']=='29:57.000' for s in steps))

    def test_raw_selection_routes_preserve_reverse_overshoots_and_copy_origins(self):
        expected=[
            ('b22-2r6c','18:13.233','2R x 6C','B22'),('b22-9c','18:13.500','1R x 9C','B22'),
            ('b22-release','18:13.833','B22','B22'),
            ('c27-7r','19:33.300','7R x 1C','C27'),
            ('d27-7r1c','19:38.267','7R x 1C','D27'),('d27-7r2c','19:38.500','7R x 2C','D27'),
            ('d27-release','19:38.833','D27','D27'),
            ('raw-a29-2r','20:07.233','2R x 1C','A29'),('raw-a29-3r','20:07.367','3R x 1C','A29'),
            ('raw-a29-release','20:07.667','A29','A29'),
            ('raw-a30-3r2c','20:09.000','3R x 2C','A30'),('raw-a30-3r3c','20:09.067','3R x 3C','A30'),
            ('raw-a30-release','20:09.633','A30','A30'),
            ('raw-c32-2r3c','20:10.133','2R x 3C','C32'),('raw-c32-3r3c','20:10.267','3R x 3C','C32'),
            ('raw-c32-release','20:10.833','C32','C32'),
            ('raw-a29b-3r2c','20:11.767','3R x 2C','A29'),('raw-a29b-2r2c','20:11.867','2R x 2C','A29'),
            ('raw-a29b-2r3c','20:12.033','2R x 3C','A29'),('raw-a29b-1r3c','20:12.200','1R x 3C','A29'),
            ('raw-a29b-release','20:12.733','A29','A29'),
            ('raw-a43-8r3c','20:18.833','8R x 3C','A43'),('raw-a43-9r3c','20:18.900','9R x 3C','A43'),
            ('raw-a43-release','20:20.033','A43','A43'),
            ('raw-a44-2r3c','20:21.467','2R x 3C','A44'),('raw-a44-1r4c','20:21.533','1R x 4C','A44'),
            ('raw-a44-1r3c','20:21.800','1R x 3C','A44'),('raw-a44-release','20:22.633','A44','A44'),
            ('raw-a23-3r3c','20:33.200','3R x 3C','A23'),('raw-a23-release','20:33.767','A23','A23'),
            ('b42long-2r5c','21:50.000','2R x 5C','B42'),('b42long-9c','21:50.233','1R x 9C','B42'),
            ('b42long-release','21:50.533','B42','B42')]
        with tempfile.TemporaryDirectory(prefix='otis-remaining-early-raw-routes-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                steps=json.loads(output.read_text())['steps'];byid={s['id']:s for s in steps}
                for suffix,time,header,anchor in expected:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        phase=byid[identifier]
                        self.assertEqual((phase['videoTime'],phase['expect']['nameBoxText']),(time,header))
                        column=0
                        for char in anchor.rstrip('0123456789'):column=column*26+ord(char)-64
                        self.assertEqual((phase['expect']['row'],phase['expect']['column']),
                            (int(anchor.lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ')),column))
                for suffix,endpoint in [('b22-2r6c','G23'),('raw-c32-2r3c','A31'),
                    ('raw-a43-9r3c','C35'),('raw-a44-2r3c','C45'),('b42long-2r5c','F43')]:
                    identifier='historical-remaining-early-'+suffix
                    if identifier in byid:self.assertEqual(byid[identifier]['action'].get('address'),endpoint)
                # The source cuts C27:C33; copying it would preserve the wrong cells.
                self.assertTrue(any(s['action']=={'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+X']}
                    and s['evidence'].get('eventId')=='history-0616' for s in steps))

    def test_discrete_visible_targets_do_not_keep_the_wrong_source_address(self):
        review=json.loads((ROOT/'docs/validation/otis-90-minute/post-attempt12-remaining-range-header-review-0-67/review.json').read_text())
        approved={r['index']:r for r in review['records']}
        with tempfile.TemporaryDirectory(prefix='otis-remaining-early-targets-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                steps=json.loads(output.read_text())['steps']
                for index,target in [(11,'A17:I17'),(45,'B19')]:
                    event=approved[index]['beforeRecord']['evidence']['eventId']
                    actual=[s for s in steps if s['evidence'].get('eventId')==event
                        and s['action'].get('type')=='text' and s['action'].get('target')=='formula.name']
                    with self.subTest(mode=mode,source=index):
                        self.assertEqual(len(actual),1)
                        self.assertEqual(actual[0]['action']['text'],target)

    def test_dense_routes_keep_overshoots_reverse_extents_and_release_headers(self):
        expected=[('date-2c','08:54.900','1R x 2C','C2'),('date-3c','08:55.067','1R x 3C','C2'),
            ('date-release','08:57.900','C2','C2'),
            ('c8-2r1c','12:50.033','2R x 1C','C8'),('c8-3r2c','12:50.100','3R x 2C','C8'),
            ('c8-4r3c','12:50.167','4R x 3C','C8'),('c8-release','12:50.533','C8','C8'),
            ('g21-2c','14:28.467','1R x 2C','G21'),('g21-3c','14:28.500','1R x 3C','G21'),
            ('g21-4c','14:28.533','1R x 4C','G21'),('g21-back3c','14:28.800','1R x 3C','G21'),
            ('g21-release','14:29.100','G21','G21'),
            ('g17-2c','14:32.767','1R x 2C','I17'),('g17-3c','14:32.800','1R x 3C','I17'),
            ('g23-2c','14:53.633','1R x 2C','G23'),('g23-2r3c','14:53.733','2R x 3C','G23'),
            ('g23-release','14:54.233','G23','G23'),
            ('c4-2c','14:59.200','1R x 2C','C4'),('c4-3c','14:59.267','1R x 3C','C4'),
            ('c4-release','14:59.833','C4','C4'),
            ('c22-2c','15:03.767','1R x 2C','C22'),('c22-2r3c','15:03.900','2R x 3C','C22'),
            ('c22-back1r3c','15:04.267','1R x 3C','C22'),('c22-release','15:04.567','C22','C22')]
        with tempfile.TemporaryDirectory(prefix='otis-remaining-early-dense-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                byid={s['id']:s for s in json.loads(output.read_text())['steps']}
                for suffix,time,header,anchor in expected:
                    with self.subTest(mode=mode,phase=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        phase=byid[identifier]
                        self.assertEqual((phase['videoTime'],phase['expect']['nameBoxText']),(time,header))
                        column=0
                        for char in anchor.rstrip('0123456789'):column=column*26+ord(char)-64
                        self.assertEqual((phase['expect']['row'],phase['expect']['column']),
                            (int(anchor.lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ')),column))
                        self.assertTrue(phase['action']['type'] in ['mouse_cell','modifier_release','key_sequence'])
                qa_id='historical-remaining-early-date-qa-settled'
                self.assertTrue(qa_id in byid,'Observed held-date Quick Analysis must have its own actual native capture')
                qa=byid[qa_id]
                self.assertEqual(qa['videoTime'],'08:55.300')
                self.assertEqual(qa['action'],{'type':'mouse','target':'worksheet.grid','gesture':'hover','dwellMs':300})
                self.assertEqual(qa['expect'],{'row':2,'column':3,'nameBoxText':'1R x 3C','quickAnalysisVisible':True})
                self.assertEqual(qa['evidence']['eventId'],'initial-055')
                self.assertTrue(qa['evidence']['sourceFrame'].endswith('/source-frames/date-C2/0040.png'))
                ordered=list(byid)
                self.assertLess(ordered.index('historical-remaining-early-date-3c'),ordered.index(qa_id))
                self.assertLess(ordered.index(qa_id),ordered.index('historical-remaining-early-date-release'))
                for suffix,address in [('g17-2c','H17'),('c22-2r3c','E21'),('c22-back1r3c','E22')]:
                    identifier='historical-remaining-early-'+suffix
                    if identifier in byid:self.assertEqual(byid[identifier]['action']['address'],address)

    def test_copy_range_and_following_origin_use_the_source_header_boundaries(self):
        with tempfile.TemporaryDirectory(prefix='otis-remaining-early-counts-') as folder:
            for mode in ['include-intro','default']:
                output=Path(folder)/(mode+'.json')
                command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),
                    '--workspace',str(Path(folder)/(mode+'-work')),'--output',str(output)]
                if mode=='include-intro':command.append('--include-intro')
                subprocess.run(command,check=True,capture_output=True)
                steps=json.loads(output.read_text())['steps'];byid={s['id']:s for s in steps}
                for suffix,time,header in [('c10-2c','12:23.633','1R x 2C'),
                    ('c10-3c','12:24.167','1R x 3C'),('c10-release','12:24.433','C10')]:
                    with self.subTest(mode=mode,source=suffix):
                        identifier='historical-remaining-early-'+suffix
                        self.assertTrue(identifier in byid,identifier)
                        phase=byid[identifier]
                        self.assertEqual((phase['videoTime'],phase['expect']['nameBoxText']),(time,header))
                        self.assertEqual((phase['expect']['row'],phase['expect']['column']),(10,3))
                        if suffix.endswith('release'):
                            self.assertEqual(phase['action'],{'type':'modifier_release','target':'worksheet.grid','keys':['Shift']})
                        else:
                            self.assertEqual(phase['action'],{'type':'key_sequence','target':'worksheet.grid','keys':['Shift+Right'],'releaseModifiers':False})
                self.assertFalse(any(s['videoTime'].startswith('12:24.')
                    and s['action'].get('target')=='formula.name' and s['action'].get('text')=='C10:E10' for s in steps))
                following=[s for s in steps if s['action']=={'type':'text','target':'formula.name','text':'C11'}
                    and '12:24.' in s['videoTime']]
                self.assertEqual(len(following),1)
                self.assertEqual(following[0]['videoTime'],'12:24.533')


if __name__=='__main__':unittest.main()
