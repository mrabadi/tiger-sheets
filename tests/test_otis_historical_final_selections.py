"""Original source range counts require genuine held native selections."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class HistoricalFinalSelections(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory(prefix='otis-final-selections-')
        cls.generated={}
        for mode in ['include-intro','default']:
            out=Path(cls.folder.name)/(mode+'.json')
            command=[sys.executable,str(ROOT/'tools/build_otis_history_manifest.py'),'--workspace',str(Path(cls.folder.name)/(mode+'-work')),'--output',str(out)]
            if mode=='include-intro':command.append('--include-intro')
            subprocess.run(command,check=True,capture_output=True)
            cls.generated[mode]=json.loads(out.read_text())['steps']

    @classmethod
    def tearDownClass(cls):cls.folder.cleanup()

    def test_five_final_range_families_use_live_dimensions_and_release_anchors(self):
        counts={'copy-source':[('2r','33:22.933','2R x 1C'),('3r','33:23.000','3R x 1C')],
            'format-destination':[('1r2c','33:23.800','1R x 2C'),('2r2c','33:23.833','2R x 2C'),('2r3c','33:23.900','2R x 3C'),('3r4c','33:23.967','3R x 4C'),('3r3c','33:24.267','3R x 3C')],
            'cashflow-fill':[('3r1c','33:53.233','3R x 1C'),('7r2c','33:53.267','7R x 2C'),('10r2c','33:53.300','10R x 2C'),('13r2c','33:53.333','13R x 2C'),('15r2c','33:53.400','15R x 2C'),('16r2c','33:53.433','16R x 2C'),('17r2c','33:53.500','17R x 2C'),('19r2c','33:53.700','19R x 2C'),('21r2c','33:53.800','21R x 2C'),('22r2c','33:54.033','22R x 2C'),('23r2c','33:54.333','23R x 2C'),('23r1c','33:54.533','23R x 1C'),('25r1c','33:54.700','25R x 1C'),('back23r1c','33:54.767','23R x 1C'),('final23r2c','33:55.367','23R x 2C'),('23r3c','33:55.433','23R x 3C')],
            'subtotal-copy':[('1r2c','34:14.667','1R x 2C'),('1r3c','34:14.733','1R x 3C')],
            'raw-header':[('1r2c','34:23.400','1R x 2C'),('2r3c','34:23.467','2R x 3C'),('1r3c','34:23.767','1R x 3C')],
            'inspection':[('2r2c','34:53.700','2R x 2C'),('3r2c','34:53.733','3R x 2C'),('4r3c','34:53.767','4R x 3C')]}
        releases={'copy-source':('33:23.233','C57'),'format-destination':('33:24.600','C62'),
            'cashflow-fill':('33:55.900','C54'),'subtotal-copy':('34:15.033','C62'),
            'raw-header':('34:24.133','C51'),'inspection':('34:54.233','C54')}
        for mode,steps in self.generated.items():
            by_id={s['id']:s for s in steps}
            for group,phases in counts.items():
                for suffix,time,count in phases:
                    identifier='historical-followup-'+group+'-'+suffix
                    with self.subTest(mode=mode,phase=identifier):
                        self.assertTrue(identifier in by_id,identifier)
                        record=by_id[identifier]
                        self.assertEqual(record['videoTime'],time)
                        self.assertEqual(record['action']['type'],'mouse_cell')
                        self.assertEqual(record['action']['gesture'],'move')
                        self.assertEqual(record['expect']['nameBoxText'],count)
            for group,(time,anchor) in releases.items():
                identifier='historical-followup-'+group+'-release'
                with self.subTest(mode=mode,release=identifier):
                    self.assertTrue(identifier in by_id,identifier)
                    record=by_id[identifier]
                    self.assertEqual((record['videoTime'],record['action']['gesture'],record['expect']['nameBoxText']),(time,'release',anchor))
            self.assertFalse(any(s['action'].get('target')=='formula.name' and s['action'].get('text')=='C56:C58' and s['videoTime']=='33:23.000' for s in steps),'The source anchor is C57, not the former coarse C56 transcription')

    def test_final_intermediate_stops_and_d8_are_preserved(self):
        stops=[('d55','34:56.567','D55'),('e54','34:57.033','E54'),('e56','34:57.533','E56')]
        for mode,steps in self.generated.items():
            by_id={s['id']:s for s in steps}
            for suffix,time,address in stops:
                identifier='historical-followup-final-'+suffix
                with self.subTest(mode=mode,stop=identifier):
                    self.assertTrue(identifier in by_id,identifier)
                    self.assertEqual((by_id[identifier]['videoTime'],by_id[identifier]['expect']['nameBoxText']),(time,address))
            with self.subTest(mode=mode,target='D8'):
                matches=[s for s in steps if s['action'].get('type')=='text' and s['action'].get('target')=='formula.name' and s['videoTime']=='35:08.933']
                self.assertEqual([s['action']['text'] for s in matches],['D8'])
        ledger=json.loads((ROOT/'docs/validation/otis-90-minute/historical-statements-visual-steps.json').read_text())
        source={s['id']:s for s in ledger['observations']}
        self.assertIn('C57:C59',source['history-1314']['observation'])
        self.assertNotIn('C56:C58',source['history-1314']['observation'])
        self.assertIn('copied',source['history-1315']['observation'])
        self.assertIn('D8',source['history-1410']['observation'])
        self.assertIn('-4439',source['history-1410']['observation'])
        self.assertIn('H12',source['history-1410']['observation'])


if __name__=='__main__':unittest.main()
