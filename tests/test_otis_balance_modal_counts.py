"""Keep the source's held range dimensions visible inside Paste Special."""
import json
from pathlib import Path
import subprocess
import sys
import unittest
from otis_late_source_restore import restore_reviewed_late_source_records

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class BalanceModalCounts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_balance_manifest.py")],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        cls.steps = json.loads((BASE / "balance-native-action-manifest.json").read_text())["steps"]

    def assert_source_count(self, event, count, released, opened, closed):
        modal = next(step for step in self.steps
                     if step["evidence"]["eventId"] == event
                     and step["action"]["type"] == "modal")
        children = modal["action"]["steps"]
        self.assertEqual(children[0].get("expect", {}).get("nameBoxText"), count,
                         "The original shows real held dimensions while Paste Special is visible")
        self.assertEqual(modal["videoTime"], opened)
        held = [step for step in self.steps[:self.steps.index(modal)]
                if step.get("expect", {}).get("nameBoxText") == count]
        self.assertTrue(held, "A real held native selection must precede the modal")
        self.assertIs(held[-1]["action"].get("releaseModifiers"), False)
        release = next(step for step in children
                       if step["action"]["type"] == "modifier_release")
        self.assertEqual(release["videoTime"], closed)
        self.assertEqual(release["action"], {
            "type": "modifier_release", "target": "pasteSpecial.dialog", "keys": ["Shift"]})
        self.assertEqual(release["expect"]["nameBoxText"], released)
        self.assertEqual(release["evidence"]["eventId"], event,
                         "The release belongs to the modal observation, not an earlier selection")
        accepted = next(step for step in children if step["action"].get("keys") == ["Return"])
        self.assertEqual(accepted["videoTime"], closed)
        self.assertLess(children.index(release), children.index(accepted))
        self.assertTrue(any("physical" in step["evidence"].get("executionMethod", "").lower()
                            for step in [held[-1], release]),
                        "The observed count does not establish the physical source keys")

    def test_three_percent_rows_stay_held_inside_legacy_paste_special(self):
        self.assert_source_count("balance-cashflow-442", "3R x 8C", "C31",
                                 "63:15.000000", "63:15.366667")

    def test_eight_balance_ratio_rows_stay_held_inside_brief_paste_special(self):
        self.assert_source_count("balance-cashflow-133", "8R x 8C", "C22",
                                 "54:09.600000", "54:09.800000")

    def test_twenty_four_ratio_rows_stay_held_inside_brief_paste_special(self):
        self.assert_source_count("balance-cashflow-199", "24R x 6C", "E59",
                                 "56:05.833333", "56:06.033333")

    def test_missing_close_keeps_both_observed_native_dialogs(self):
        modal = next(step for step in self.steps
                     if step["evidence"]["eventId"] == "balance-cashflow-110")
        self.assertTrue(modal["action"].get("dialogSequence"),
                        "The source rejects a real correction prompt before the ordinary warning")
        children = modal["action"]["steps"]
        self.assertEqual(modal["videoTime"], "53:41.566667")
        self.assertEqual(children[0]["expect"]["modal"], "worksheet.formulaCorrection")
        self.assertEqual(children[1]["action"]["keys"], ["Escape"])
        self.assertEqual(children[2]["videoTime"], "53:42.466667")
        self.assertEqual(children[-1]["videoTime"], "53:42.800000")
        self.assertEqual(children[-1]["action"]["keys"], ["Return"])
        self.assertEqual(modal["expect"]["formulaDraft"], "=-D69/SUM(D40:D41")

    def test_twenty_three_cashflow_rows_stay_held_inside_brief_paste_special(self):
        self.assert_source_count("balance-cashflow-558", "23R x 6C", "E98",
                                 "65:41.833333", "65:42.000000")

    def test_other_balance_source_events_are_unchanged(self):
        reference = json.loads((BASE / "prepared-manifest-history/105a3987-before-post-attempt9-repairs/segments/balance-native-action-manifest.json").read_text())["steps"]
        # Each listed event has a separate source/native regression. The
        # preserved105a baseline still covers every other action and order.
        owned = {"balance-cashflow-110", "balance-cashflow-132",
                 "balance-cashflow-133", "balance-cashflow-199",
                 "balance-cashflow-200", "balance-cashflow-441",
                 "balance-cashflow-442", "balance-cashflow-557",
                 "balance-cashflow-558"} | {
                     "balance-cashflow-" + str(n) for n in range(187, 192)}
        # Independently pinned dense held-selection follow-up; exact phases,
        # copy disproof and outside-event invariance have their own regressions.
        owned |= {"balance-cashflow-" + str(n) for n in
                  [134,135,136,194,195,196,197,198,439,440,551,552,553,554,555,556]}
        self.assertEqual([step for step in restore_reviewed_late_source_records(self.steps)
                          if step["evidence"]["eventId"] not in owned],
                         [step for step in reference if step["evidence"]["eventId"] not in owned])

    def test_observed_post_modal_anchors_are_retained(self):
        for event, cell, time in [("balance-cashflow-442", "C32", "63:15.533333"),
                                  ("balance-cashflow-558", "E99", "65:42.100000"),
                                  ("balance-cashflow-558", "E100", "65:42.366667")]:
            selected = [step for step in self.steps if step["evidence"]["eventId"] == event
                        and step["action"] == {"type": "mouse_cell", "address": cell, "gesture": "click"}]
            self.assertEqual(len(selected), 1)
            self.assertEqual(selected[0]["videoTime"], time)
            self.assertEqual(selected[0]["expect"]["nameBoxText"], cell)


if __name__ == "__main__":
    unittest.main()
