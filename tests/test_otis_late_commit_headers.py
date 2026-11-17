"""Observed late Ready headers remain distinct from a plain Enter-down adapter."""
import copy
import json
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import unittest
from otis_late_source_restore import (restore_reviewed_adjacent_source_records,
                                      restore_reviewed_late_source_records)


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"
PROOF = BASE / "native-history/post-attempt12-late-source-headers"
REVIEW = json.loads((PROOF / "approved-readonly-review.json").read_text())
CONTRACT = json.loads((PROOF / "source-order-contract.json").read_text())


def column_number(address):
    result = 0
    for letter in re.match(r"[A-Z]+", address)[0]:
        result = result * 26 + ord(letter) - ord("A") + 1
    return result


class LateCommitHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generated = {}
        for segment in ("income", "balance", "linking"):
            subprocess.run([sys.executable, str(ROOT / f"tools/build_otis_{segment}_manifest.py")],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
            cls.generated[segment] = json.loads((BASE / f"{segment}-native-action-manifest.json").read_text())["steps"]

    def verify_segment(self, segment, steps):
        steps = restore_reviewed_adjacent_source_records(steps, segment)
        prefix = {"income": "income-", "balance": "balance-cashflow-", "linking": "linking-debt-checks-"}[segment]
        self.assertEqual(len(CONTRACT["records"]), 151)
        self.assertEqual({row["id"] for row in CONTRACT["records"][:74]},
                         {row["id"] for row in REVIEW["proposedOverrides"]})
        remaining = json.loads((PROOF / "remaining51-reviewed-source-contract.json").read_text())
        self.assertEqual(len(remaining["records"]), 51)
        self.assertEqual(CONTRACT["records"][74:125], remaining["records"])
        retired = set(CONTRACT["classification"]["retiredInventedCommits"])
        self.assertEqual({row["id"] for row in CONTRACT["records"]},
                         {row["id"] for row in REVIEW["all154ScreenedRows"]} - retired)
        self.assertEqual(len(retired), 3)
        required = [row for row in CONTRACT["records"] if row["id"].startswith(prefix)]
        self.assertTrue(required)
        current = {step["id"]: step for step in steps}
        before = json.loads((PROOF / "before" / f"{segment}-native-action-manifest.json").read_text())["steps"]
        original = {step["id"]: step for step in before}
        def source_seconds(step):
            return sum(float(part) * 60 ** power for power, part in
                       enumerate(reversed(step["videoTime"].split(":"))))
        for row in required:
            with self.subTest(id=row["id"]):
                actual = current[row["id"]]
                address = row["sourceNameBox"]
                self.assertEqual(actual["action"], {
                    "type": "key_sequence", "target": original[row["id"]]["action"]["target"],
                    "keys": row["keys"],
                }, "Plain Return must not replace the source's independently observed retained/rightward Ready header")
                for name, value in {"nameBoxText": address,
                                    "row": int(re.search(r"\d+", address)[0]),
                                    "column": column_number(address)}.items():
                    self.assertEqual(actual.get("expect", {}).get(name), value)
                self.assertEqual(actual["expect"]["cells"], original[row["id"]]["expect"]["cells"],
                                 "Header refinement must preserve every original input/value/style expectation")
                method = actual["evidence"].get("executionMethod", "").lower()
                self.assertIn("inferred", method)
                self.assertIn("physical", method)
                self.assertIn("unobserved", method)
                if "videoTime" in row:
                    self.assertEqual(actual["videoTime"], row["videoTime"],
                                     "Only independently decoded source boundaries retime a commit")
                if "sourceEventId" in row:
                    self.assertEqual(actual["evidence"]["eventId"], row["sourceEventId"],
                                     "A disproved coarse entry must join its actual observed operation")
                index = next(i for i, step in enumerate(before) if step["id"] == row["id"])
                prior_commit = max((i for i, step in enumerate(before[:index])
                                    if step["id"] in {entry["id"] for entry in CONTRACT["records"]}), default=-1)
                for preceding in before[prior_commit + 1:index]:
                    if (preceding["evidence"]["eventId"] == original[row["id"]]["evidence"]["eventId"]
                            and preceding["id"] in current):
                        self.assertLessEqual(source_seconds(current[preceding["id"]]), source_seconds(actual),
                                             preceding["id"] + " cannot occur after its own commit")
        allowed = {row["id"] for row in required}
        for row in CONTRACT["adjacentChanges"]:
            if not row["id"].startswith(prefix):
                continue
            allowed.add(row["id"])
            actual = current[row["id"]]
            self.assertEqual(actual["action"], row.get("action", original[row["id"]]["action"]),
                             "Only explicitly reviewed adjacent action counterparts may change")
            self.assertEqual(actual["videoTime"], row["videoTime"])
            for field, value in row.get("expect", {}).items():
                self.assertEqual(actual.get("expect", {}).get(field), value)
        for row in CONTRACT["addedRecords"]:
            if not row["id"].startswith(prefix):
                continue
            allowed.add(row["id"])
            self.assertIn(row["id"], current,
                          "A verified source return, pending-pointer correction or actual Saving phase needs a native action")
            actual = current[row["id"]]
            self.assertEqual(actual["action"], row["action"])
            self.assertEqual(actual["videoTime"], row["videoTime"])
            self.assertEqual(actual.get("expect"), row["expect"])
            self.assertEqual(actual["evidence"]["eventId"], row["eventId"])
        for row in CONTRACT.get("removedRecords", []):
            if not row["id"].startswith(prefix):
                continue
            allowed.add(row["id"])
            self.assertNotIn(row["id"], current, "An unobserved duplicate drag or disproved Fill operation must not remain")
        source_proofs = json.loads((PROOF / "approved-source-ledger-event-changes.json").read_text())
        source_changes = {item["id"]: item for ledger in source_proofs["ledgers"]
                          for item in ledger["changes"]}
        unchanged_actions = copy.deepcopy([step for step in steps if step["id"] not in allowed])
        for step in unchanged_actions:
            change = source_changes.get(step["evidence"]["eventId"])
            if change:
                self.assertEqual(step["evidence"]["observed"], change["after"]["observed"])
                step["evidence"]["observed"] = change["before"]["observed"]
        self.assertEqual(unchanged_actions,
                         [step for step in before if step["id"] not in allowed],
                         "Every unrelated action/order remains exact; only the named source prose may be corrected")
        ledger_names = {"income": "income-projections-visual-steps.json",
                        "balance": "balance-cashflow-visual-steps.json",
                        "linking": "linking-debt-checks-visual-steps.json"}
        approved_ids = {"income": {181, 243, 266}, "balance": {125},
                        "linking": {36, 37, 39, 40, 169, 170, 171, 173,
                                    247, 248, 249, 401, 402, 403}}
        proofs = json.loads((PROOF / "approved-source-ledger-event-changes.json").read_text())
        proof = next(item for item in proofs["ledgers"]
                     if Path(item["path"]).name == ledger_names[segment])
        baseline = Path(proof["beforePath"])
        self.assertEqual(hashlib.sha256(baseline.read_bytes()).hexdigest(), proof["beforeSha256"])
        changes = {item["id"]: item for item in proof["changes"]}
        self.assertEqual({int(identifier.split("-")[-1]) for identifier in changes},
                         approved_ids[segment], "Only explicitly reviewed coarse source claims are superseded")
        prior = json.loads(baseline.read_text())
        corrected = copy.deepcopy(prior)
        for index, event in enumerate(corrected["events"]):
            if event["id"] in changes:
                self.assertEqual(event, changes[event["id"]]["before"])
                corrected["events"][index] = changes[event["id"]]["after"]
        self.assertEqual(json.loads((BASE / ledger_names[segment]).read_text()), corrected,
                         "Disproved copy/fill/typing claims must be corrected; unrelated source objects stay exact")

    def check_with_unrelated_mutation_counterproof(self, segment):
        self.verify_segment(segment, self.generated[segment])
        restored = restore_reviewed_late_source_records(self.generated[segment], segment)
        prior = json.loads((PROOF / "before" / f"{segment}-native-action-manifest.json").read_text())["steps"]
        self.assertEqual(restored, prior)
        changed_id = next(row["id"] for row in CONTRACT["records"]
                          if row["id"] in {step["id"] for step in self.generated[segment]})
        forged_approved = copy.deepcopy(self.generated[segment])
        next(step for step in forged_approved if step["id"] == changed_id)["action"]["keys"] = ["Return"]
        with self.assertRaises(AssertionError):
            restore_reviewed_late_source_records(forged_approved, segment)
        wrong_order = copy.deepcopy(self.generated[segment])
        wrong_order[0], wrong_order[1] = wrong_order[1], wrong_order[0]
        with self.assertRaises(AssertionError):
            restore_reviewed_late_source_records(wrong_order, segment)
        damaged = copy.deepcopy(self.generated[segment])
        allowed = {row["id"] for field in ["records", "adjacentChanges", "addedRecords", "removedRecords"]
                   for row in CONTRACT[field]}
        unrelated = next(step for step in damaged if step["id"] not in allowed)
        unrelated["action"] = {"type": "text", "target": "worksheet.grid", "text": "forged"}
        with self.assertRaises(AssertionError):
            self.verify_segment(segment, damaged)

    def test_income_ready_headers(self):
        self.check_with_unrelated_mutation_counterproof("income")

    def test_balance_ready_headers(self):
        self.check_with_unrelated_mutation_counterproof("balance")

    def test_linking_ready_headers(self):
        self.check_with_unrelated_mutation_counterproof("linking")


if __name__ == "__main__":
    unittest.main()
