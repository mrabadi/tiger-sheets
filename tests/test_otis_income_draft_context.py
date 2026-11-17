"""The source's pending C12 formula survives its actual PDF detour."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class IncomeDraftContextTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_income_manifest.py")], cwd=ROOT, check=True, capture_output=True)
        cls.steps = json.loads((BASE / "income-native-action-manifest.json").read_text())["steps"]

    def test_B13_visible_label_prefixes_remain_uncommitted_before_Home(self):
        observed = [(step["videoTime"], step.get("expect", {}).get("formulaDraft"))
                    for step in self.steps if step["evidence"]["eventId"] == "income-178"
                    and "formulaDraft" in step.get("expect", {})]
        self.assertEqual(observed, [("44:15", "Gr"), ("44:16", "Gro"), ("44:17", "Growth Rate:")],
                         "Original0545/0546/0547 retain three distinct real inline Enter drafts")
        self.assertFalse(any(step["id"] == "income-178-commit-B13" for step in self.steps),
                         "The old unobserved Return moved B13 to B14 before Home; remove it rather than retaining a fake action")
        for step in self.steps:
            if step["evidence"]["eventId"] == "income-178" and "formulaDraft" in step.get("expect", {}):
                self.assertEqual(step["action"]["target"], "worksheet.cellEditor")
                self.assertEqual((step["expect"]["row"], step["expect"]["column"]), (13, 2))
                self.assertEqual(step["expect"]["cells"], [{"address": "B13", "input": ""}])

    def test_Home_peek_commits_by_focus_without_advancing_the_source_B13(self):
        by_id = {step["id"]: step for step in self.steps}
        peek = by_id["income-179-peek-home"]
        self.assertEqual(peek["action"], {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Alt", "H"]})
        for field, value in {"row": 13, "column": 2, "nameBoxText": "B13", "formulaDraft": "Growth Rate:"}.items():
            self.assertEqual(peek.get("expect", {}).get(field), value,
                             "Source0548 opens Home in Ready with B13 retained; the actual inferred grid-focus route must preserve it")
        self.assertIn({"address": "B13", "input": "Growth Rate:"}, peek["expect"]["cells"])
        self.assertIn("inferred", peek["evidence"]["executionMethod"])
        for identifier in ["income-179-collapse-home", "income-179-select-expose-C9", "income-179-select-C9"]:
            self.assertEqual(by_id[identifier]["videoTime"], "44:19", "Source0549 is the first retained collapsed/C9 frame")

    def test_C12_commits_only_after_the_observed_PDF_return(self):
        commits = [step for step in self.steps if step["id"].endswith("commit-formula-C12")]
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0]["videoTime"], "45:21.400", "Native30fps source retains Point through45:21.366667; firstReadyC12/result is45:21.4")
        self.assertEqual(commits[0]["evidence"]["eventId"], "income-193")
        self.assertEqual(commits[0]["action"], {"type": "key_sequence", "target": "worksheet.cellEditor", "keys": ["Ctrl+Return"]}, "Source retainsC12; CtrlEnter is an explicitly inferred commit method")
        for field, value in {"row":12,"column":3,"nameBoxText":"C12"}.items():
            self.assertEqual(commits[0]["expect"][field], value)

    def test_both_sides_of_the_context_detour_assert_the_uncommitted_draft(self):
        for time in ["45:12", "45:20"]:
            with self.subTest(time=time):
                pending = [step for step in self.steps if step["videoTime"] == time and step.get("expect", {}).get("formulaDraft") == "=C22/C9"]
                self.assertEqual(len(pending), 1, "The actual source pending formula needs read-only evidence on both sides of PDF activation")
                self.assertIn({"address": "C12", "input": ""}, pending[0]["expect"]["cells"])
                self.assertEqual(pending[0]["expect"].get("nameBoxText"), "C9", "Source local arithmeticPoint preserves the referenced-cell NameBox on both sides of PDF activation")
                self.assertEqual(pending[0]["action"], {"type": "key_sequence", "target": "worksheet.grid", "keys": []}, "An observational checkpoint must not manufacture focus, typing, or an extra worksheet edit")


    def test_C12_historical_fill_keeps_two_then_three_held_columns(self):
        steps = [step for step in self.steps if step["evidence"]["eventId"] == "income-194"]
        counts = [step["expect"]["nameBoxText"] for step in steps if "R x " in step.get("expect", {}).get("nameBoxText", "")]
        self.assertEqual(counts, ["1R x 2C", "1R x 3C", "1R x 3C"], "Native30fps source45:21.933333→45:22.7 retains both held dimensions")
        release = next(step for step in steps if step["action"].get("gesture") == "release")
        fill = next(step for step in steps if step["action"].get("keys") == ["Ctrl+R"])
        self.assertEqual(release["videoTime"], "45:22.733333")
        self.assertEqual(fill["videoTime"], "45:22.733333")
        self.assertLess(steps.index(release), steps.index(fill))
        self.assertEqual(fill["expect"]["nameBoxText"], "C12")

if __name__ == "__main__":
    unittest.main()
