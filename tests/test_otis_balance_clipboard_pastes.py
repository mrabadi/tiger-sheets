"""Three actual source PasteSpecial dialogs and their clipboard provenance."""
import json
from pathlib import Path
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'

class BalanceClipboardPastes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable,str(ROOT/'tools/build_otis_balance_manifest.py')],cwd=ROOT,check=True,capture_output=True)
        cls.steps=json.loads((BASE/'balance-native-action-manifest.json').read_text())['steps']

    def source(self,event):
        return [s for s in self.steps if s['evidence']['eventId']==f'balance-cashflow-{event:03d}']

    def test_three_actual_dialogs_retain_all_then_two_formulas_choices(self):
        modes=[]
        for event in [187,189,191]:
            modals=[s for s in self.source(event) if s['action']['type']=='modal']
            self.assertEqual(len(modals),1,'Original55:49.3/50.8/52.6 each contains an actual PasteSpecial dialog')
            modal=modals[0]
            modes.append(modal['action']['steps'][0]['action']['keys'])
            self.assertEqual(modal['action']['steps'][-1]['action']['keys'],['Return'])
        self.assertEqual(modes,[['A'],['F'],['F']])

    def test_f73_uses_the_current_f70_clipboard_source(self):
        interval=[s for s in self.steps if s['evidence']['eventId'] in ['balance-cashflow-189','balance-cashflow-190','balance-cashflow-191']]
        copied=[i for i,s in enumerate(interval) if s['action'].get('keys')==['Ctrl+C']]
        self.assertEqual(len(copied),1,'Keep source F69 clipboard through the F70 paste, then copy the observed F70 source once')
        copy=interval[copied[0]]
        self.assertEqual(copy.get('expect',{}).get('nameBoxText'),'F70','Original3352.666667 has F70 marching ants under the F73 dialog')
        self.assertFalse(any(s['action'].get('address')=='F69' for s in interval[copied[0]:]))

    def test_intermediate_contract_debt_and_lease_anchors_are_preserved(self):
        navigation=[s['expect']['nameBoxText'] for s in self.source(190) if s['action'].get('keys')==['Down']]
        self.assertEqual(navigation,['F71','F72','F73'],'Dense55:51.866/52.066/52.233 shows each intermediate active cell')

    def test_f73_original_formula_is_retained_before_later_f2_correction(self):
        checked=[s for s in self.source(191) if s['action'].get('keys')==['F2']]
        self.assertEqual(len(checked),1)
        self.assertEqual(checked[0].get('expect',{}).get('formulaDraft'),'=-SUM(F$40:F$42)*F31')
        self.assertEqual(checked[0]['videoTime'],'55:53.300000')

if __name__=='__main__':unittest.main()
