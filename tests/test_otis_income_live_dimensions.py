"""Keep the source's visible held dimensions, anchors and copy feedback."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class IncomeSourceLiveDimensions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_income_manifest.py")], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        cls.manifest = json.loads((BASE / "income-native-action-manifest.json").read_text())

    def source(self, event):
        return [step for step in self.manifest["steps"] if step["evidence"]["eventId"] == event]

    def counts(self, event):
        return [step["expect"]["nameBoxText"] for step in self.source(event)
                if "R x " in step.get("expect", {}).get("nameBoxText", "")]

    def test_header_preserves_visible_overshoot_and_delays_copy_until_release(self):
        self.assertEqual(self.counts("income-047"), ["1R x 3C", "2R x 5C", "2R x 6C", "2R x 7C", "2R x 8C", "1R x 9C", "1R x 9C"])
        copied = self.source("income-048")
        self.assertFalse(any(step["action"].get("gesture") == "press" for step in copied), "The already selected header must not be re-dragged before Copy")
        self.assertEqual(copied[0]["action"]["keys"], ["Ctrl+C"])
        self.assertGreater(copied[0]["videoTime"], self.source("income-047")[-1]["videoTime"])

    def test_other_income_and_reverse_tax_selection_keep_all_held_dimensions(self):
        self.assertEqual(self.counts("income-217"), ["2R x 1C", "2R x 3C", "2R x 4C", "3R x 5C", "4R x 4C", "2R x 4C"])
        tax = next(step for step in self.source("income-217") if step["action"].get("address") == "B41" and step["action"].get("gesture") == "move")
        self.assertEqual(tax["expect"], {"row": 42, "column": 5, "nameBoxText": "2R x 4C"})

    def test_pension_reduction_is_one_continuously_held_drag(self):
        self.assertEqual(self.counts("income-218"), ["2R x 2C", "2R x 4C", "1R x 4C"])
        gestures = [step["action"].get("gesture") for step in self.source("income-218") if step["action"]["type"] == "mouse_cell"]
        self.assertEqual(gestures, ["click", "press", "move", "move", "move", "release"])

    def test_net_income_keeps_intermediate_width_and_long_held_state(self):
        self.assertEqual(self.counts("income-227"), ["2R x 2C", "2R x 3C", "2R x 4C", "1R x 4C", "1R x 4C"])
        released = self.source("income-227")[-1]
        self.assertEqual(released["videoTime"], "47:33.250")
        self.assertEqual(released["expect"]["nameBoxText"], "B44")

    def test_forecast_copy_has_real_held_shift_and_paste_before_calculation(self):
        self.assertEqual(self.counts("income-269"), ["6R x 1C", "11R x 1C", "12R x 2C", "13R x 2C", "14R x 1C", "15R x 1C", "15R x 2C", "15R x 3C", "15R x 4C", "15R x 5C", "15R x 5C"])
        actions = [step["action"] for step in self.source("income-269")]
        self.assertEqual(sum(action.get("keys") == ["Ctrl+C"] for action in actions), 1)
        shift = [action for action in actions if action.get("keys") == ["Shift+Right"]]
        self.assertEqual(len(shift), 4)
        self.assertTrue(all(action["releaseModifiers"] is False for action in shift))
        self.assertEqual(actions[-2], {"type": "modifier_release", "target": "worksheet.grid", "keys": ["Shift"]})
        self.assertEqual(actions[-1]["keys"], ["Ctrl+V"])
        self.assertFalse(any(action.get("keys") == ["Ctrl+R"] for action in actions))

    def test_source_save_retains_the_observed_cash_flows_view(self):
        source = self.source("income-063")
        saved = next((step for step in source if step["action"].get("keys") == ["Ctrl+S"]), None)
        self.assertIsNotNone(saved, "The actual source Save must persist the cashflows E51/top37 view")
        for field, expected in {"documentName": "Otis-10-K-Raw.xls", "sheetName": "cash flows", "row": 51, "column": 5, "topRow": 37, "firstColumn": 1, "nameBoxText": "E51"}.items():
            self.assertEqual(saved["expect"][field], expected)

    def test_growth_range_dimensions_remain_held_inside_alignment_dialog(self):
        selection = self.source("income-075")
        clicked = [step for step in selection if step["action"].get("address") == "C5"
                   and step["action"].get("gesture") == "click"]
        self.assertEqual(len(clicked), 1)
        self.assertEqual(clicked[0]["videoTime"], "39:18")
        held = [step for step in selection if step.get("expect", {}).get("nameBoxText") == "1R x 3C"]
        self.assertEqual(len(held), 1, "Source39:19 retains the actual1Rx3C header before its modal")
        self.assertEqual(held[0]["videoTime"], "39:19")
        self.assertEqual(held[0]["action"], {
            "type": "key_sequence", "target": "worksheet.grid",
            "keys": ["Shift+Right", "Shift+Right"], "releaseModifiers": False,
        })
        self.assertFalse(any(step["action"].get("gesture") == "release" for step in selection),
                         "Do not release the range before the source39:20 modal header")
        modal = next(step for step in self.source("income-076") if step["action"]["type"] == "modal")
        children = modal["action"]["steps"]
        alignment = next(step for step in children if step["action"].get("tab") == "Alignment")
        self.assertEqual(alignment["videoTime"], "39:20")
        self.assertEqual(alignment["expect"]["nameBoxText"], "1R x 3C")
        release = next(step for step in children if step["action"]["type"] == "modifier_release")
        self.assertEqual(release["videoTime"], "39:21")
        self.assertEqual(release["action"], {
            "type": "modifier_release", "target": "formatCells.dialog", "keys": ["Shift"],
        })
        self.assertEqual(release["expect"]["nameBoxText"], "C5")
        self.assertLess(children.index(alignment), children.index(release))
        dismissed = next(step for step in children if step["action"].get("keys") == ["Escape"])
        self.assertEqual(dismissed["videoTime"], "39:21")
        self.assertLess(children.index(release), children.index(dismissed))
        after = [step for step in self.source("income-076") if step["action"].get("address") == "D5"]
        self.assertEqual(len(after), 2)
        self.assertTrue(all(step["videoTime"] == "39:21" for step in after))


if __name__ == "__main__":
    unittest.main()
