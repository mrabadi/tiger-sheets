import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class CompatibilityCatalogTests(unittest.TestCase):
    def test_every_frozen_entry_has_a_unique_evidence_identity(self):
        for catalog, key in [('functions', 'name'), ('shortcuts', 'id'), ('features', 'id')]:
            with self.subTest(catalog=catalog):
                data = json.loads((ROOT / 'compatibility' / f'{catalog}.json').read_text())
                self.assertEqual(data['schemaVersion'], 1)
                identities = [entry[key] for entry in data['entries']]
                self.assertEqual(len(identities), len(set(identities)), 'distinct reference rows need distinct evidence identities')

if __name__ == '__main__':
    unittest.main()
