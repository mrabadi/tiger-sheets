"""Retain independently observed balance-sheet draft corrections, not replacements."""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import unittest
from otis_late_source_restore import restore_reviewed_late_source_records


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class BalanceFormulaEditStates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_balance_manifest.py")],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        cls.steps = json.loads((BASE / "balance-native-action-manifest.json").read_text())["steps"]

    def event(self, identifier):
        return [step for step in self.steps if step["evidence"]["eventId"] == identifier]

    def test_denominator_selects_only_the_endpoint_then_preserves_partial_D4(self):
        selected = self.event("balance-cashflow-112")
        self.assertTrue(any(step["action"].get("keys") == ["End", "Shift+Left", "Shift+Left", "Shift+Left"]
                            for step in selected),
                        "53:44 selects only D41; selecting the complete formula skips the original endpoint state")
        partial = self.event("balance-cashflow-113")
        self.assertTrue(any(step.get("expect", {}).get("formulaDraft") == "=-D69/SUM(D40:D4"
                            for step in partial), "53:45 visibly retains the partial endpoint D4")
        self.assertFalse(any(step["action"].get("keys") == ["Ctrl+A"] for step in selected + partial))
        self.assertFalse(any(step["action"].get("text", "").startswith("=") for step in selected + partial))
        self.assertTrue(all("adapt" in step["evidence"].get("executionMethod", "").lower()
                            for step in selected + partial), "The sampled effect does not prove physical editing keys")

    def test_other_liabilities_keeps_partial_F_autocomplete_then_actual_F29_point(self):
        edited = self.event("balance-cashflow-192")
        self.assertTrue(any(step.get("expect", {}).get("formulaDraft") == "=-SUM(F$40:F$42)*F"
                            for step in edited), "55:56 shows a partial F token and native F autocomplete")
        partial = next(step for step in edited if step.get("expect", {}).get("formulaDraft") == "=-SUM(F$40:F$42)*F")
        self.assertEqual(partial["action"].get("target"), "worksheet.cellEditor",
                         "Source55:56 edits the offscreen inline F73 editor; its native completion list is anchored there")
        self.assertTrue(any(step["action"] == {"type": "vertical_view", "topRow": 22}
                            for step in edited[:edited.index(partial)]),
                        "The source exposes top row22 before the temporary F-token completion list appears")
        pointed = [step for step in edited if step["action"].get("type") == "mouse_cell"
                   and step["action"].get("address") == "F29"]
        self.assertEqual(len(pointed), 1, "55:57 must really point at F29, producing the native red reference rectangle")
        self.assertEqual(pointed[0]["videoTime"], "55:57")
        self.assertEqual(pointed[0].get("expect", {}).get("formulaDraft"), "=-SUM(F$40:F$42)*F29")
        self.assertFalse(any(step["action"].get("keys") == ["Ctrl+A"] for step in edited))
        self.assertFalse(any(step["action"].get("text", "").startswith("=") for step in edited))
        self.assertTrue(all("adapt" in step["evidence"].get("executionMethod", "").lower()
                            for step in edited))

    def test_both_final_formulas_remain_exact(self):
        for identifier, address, formula in [
            ("balance-cashflow-114", "D27", "=-D69/SUM(D40:D42)"),
            ("balance-cashflow-193", "F73", "=-SUM(F$40:F$42)*F29"),
        ]:
            self.assertTrue(any({"address": address, "input": formula} in step.get("expect", {}).get("cells", [])
                                for step in self.event(identifier)))

    def test_source_ready_commits_retain_the_observed_cell(self):
        for identifier, address, row, column in [
            ("balance-cashflow-114", "D27", 27, 4),
            ("balance-cashflow-193", "F73", 73, 6),
        ]:
            committed = next(step for step in self.event(identifier)
                             if step["action"].get("type") == "key_sequence")
            self.assertEqual(committed["action"]["keys"], ["Ctrl+Return"],
                             "The original Ready frame retains the formula cell; Return moves to another row")
            for name, expected in [("row", row), ("column", column), ("nameBoxText", address)]:
                self.assertEqual(committed["expect"].get(name), expected)
            method = committed["evidence"].get("executionMethod", "").lower()
            self.assertIn("adapt", method)
            self.assertIn("physical", method)
            self.assertIn("not observable", method)

    def test_ready_percentage_formula_bar_matches_original_65_02(self):
        selected = self.event("balance-cashflow-523")
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["videoTime"], "65:02")
        self.assertEqual(selected[0]["action"], {
            "type": "mouse", "target": "tutorial.fontPalette",
            "childAccessibleName": "Font color #0000FF"})
        self.assertEqual(selected[0].get("expect", {}).get("formulaDraft"), "37.5%",
                         "Original frame0891 shows G39's percentage in the Ready formula bar, not raw0.375")
        self.assertTrue(any(frame.endswith("frame-0891.jpg")
                            for frame in selected[0]["evidence"]["frames"]))

    def test_no_other_balance_actions_are_rewritten(self):
        changed = {"balance-cashflow-112", "balance-cashflow-113", "balance-cashflow-114",
                   "balance-cashflow-192", "balance-cashflow-193"}
        reference = json.loads((BASE / "prepared-manifest-history/105a3987-before-post-attempt9-repairs/segments/balance-native-action-manifest.json").read_text())["steps"]
        def normalized(records, excluded):
            retained = json.loads(json.dumps([step for step in records
                                             if step["evidence"]["eventId"] not in excluded]))
            for step in retained:
                if step["id"] == "balance-cashflow-523-recent-blue-swatch":
                    expectation = step.get("expect", {})
                    expectation.pop("formulaDraft", None)
                    if not expectation:
                        step.pop("expect", None)
            return retained
        # Preserve the original digest against its actual preserved baseline.
        original = normalized(reference, changed)
        self.assertEqual(len(original), 1619)
        encoded = json.dumps(original, sort_keys=True, separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "767e247d17c7df68fa56273e5f6a5cd5520d6bc28cbc62d79b4992ecc35499b5",
                         "The original unrelated-action baseline must remain unchanged")
        # Each explicitly approved count/correction/clipboard event has an
        # independent permanent source test and actual native proof.
        approved = changed | {"balance-cashflow-441", "balance-cashflow-442",
                              "balance-cashflow-557", "balance-cashflow-558",
                              "balance-cashflow-110", "balance-cashflow-132",
                              "balance-cashflow-133", "balance-cashflow-199",
                              "balance-cashflow-200"} | {
                                  "balance-cashflow-" + str(n) for n in range(187, 192)}
        approved |= {"balance-cashflow-" + str(n) for n in
                     [134,135,136,194,195,196,197,198,439,440,551,552,553,554,555,556]}
        expected = normalized(reference, approved)
        self.assertEqual(normalized(restore_reviewed_late_source_records(self.steps), approved), expected)
        changed_child = json.loads(json.dumps(self.steps))
        modal = next(step for step in changed_child
                     if step["evidence"]["eventId"] not in approved
                     and step["action"]["type"] == "modal")
        modal["action"]["steps"][0]["action"] = {
            "type": "key_sequence", "target": "@active_modal", "keys": ["Escape"]}
        with self.assertRaisesRegex(AssertionError, "Unapproved action/order: exact reviewed adjacent after-image required"):
            restore_reviewed_late_source_records(changed_child)
        reordered = json.loads(json.dumps(expected))
        reordered[0], reordered[1] = reordered[1], reordered[0]
        self.assertNotEqual(reordered, expected, "Unapproved record order is significant")


if __name__ == "__main__":
    unittest.main()
