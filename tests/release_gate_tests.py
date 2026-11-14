import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('release_gate', Path(__file__).parents[1] / 'tools/release_gate.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class ReleaseGateTests(unittest.TestCase):
    def test_missing_formula_evidence_blocks_release(self):
        missing = gate.missing_evidence({'entries': [{'name': 'XIRR', 'scope': 'required'}]}, {'XIRR': {'passed': False}}, 'name')
        self.assertEqual(missing, ['XIRR'])
    def test_excluded_entries_do_not_block(self):
        self.assertEqual(gate.missing_evidence({'entries': [{'name': 'RTD', 'scope': 'excluded'}]}, {}, 'name'), [])

    def test_pass_boolean_alone_is_not_executable_evidence(self):
        self.assertEqual(gate.missing_evidence({'entries': [{'name': 'SUM', 'scope': 'required'}]}, {'SUM': {'passed': True}}, 'name'), ['SUM'])

    def test_evidence_references_must_be_named_executed_cases(self):
        self.assertFalse(gate.valid_evidence({'passed': True, 'test': True, 'cases': True}))
        self.assertFalse(gate.valid_evidence({'passed': True, 'test': 'formula_tests', 'cases': [False]}))

if __name__ == '__main__':
    unittest.main()
