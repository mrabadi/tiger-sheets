"""Source-clock ordering for actual nested income replay actions."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def seconds(time):
    total = 0
    for part in time.split(":"):
        total = total * 60 + float(part)
    return total


class IncomeSourceClockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_income_manifest.py")], cwd=ROOT, check=True, capture_output=True)
        cls.manifest = json.loads((ROOT / "docs/validation/otis-90-minute/income-native-action-manifest.json").read_text())

    def test_source_time_order_is_monotonic_inside_every_modal(self):
        def visit(records):
            for previous, current in zip(records, records[1:]):
                self.assertGreaterEqual(seconds(current["videoTime"]), seconds(previous["videoTime"]), "Nested source clock must keep actual action order: " + previous["id"] + " -> " + current["id"])
            for record in records:
                visit(record["action"].get("steps", []))
        visit(self.manifest["steps"])

    def test_colors_acceptance_uses_verified_first_close_fade(self):
        modal = next(step for step in self.manifest["steps"] if step["id"] == "income-067-more-colors-dialog")
        children = modal["action"]["steps"]
        acceptance = next(step for step in children if step["id"] == "income-068-accept-blue")
        self.assertEqual(acceptance["videoTime"], "38:54.900", "Original30fps source is still fully modal at38:54.866667 and first fades at38:54.9")
        self.assertEqual([step["action"]["text"] for step in children if step["action"]["type"] == "text"], ["2", "5", "5"], "The correction must preserve each observed RGB input action")


if __name__ == "__main__":
    unittest.main()
