"""Check that source qualifications survive the executable manifest join."""
import json
import ast
import hashlib
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class FullManifestSourceQualifications(unittest.TestCase):
    def test_history_atomic_namebox_gap_is_retained_without_fabricated_input(self):
        manifest = json.loads((BASE / "full-native-action-manifest.json").read_text())
        history = next(segment for segment in manifest["segments"] if Path(segment["manifest"]).name == "historical-native-actions.json")
        source = json.loads(Path(history["manifest"]).read_text())
        self.assertTrue(source["knownNativeStateGaps"], "The reviewed history explicitly qualifies the unexplained source A100 state")
        self.assertEqual(history.get("knownNativeStateGaps"), source["knownNativeStateGaps"], "Joining segments must preserve source-state gaps, not silently discard their qualification")


class ReviewedSemanticCoverageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        (self.folder / "historical-statements-visual-steps.json").write_text(json.dumps({"observations": [{"id": "history-0001", "videoSeconds": 564, "observation": "Held source view"}]}))
        self.action = {"id": "actual-native-action", "videoTime": "09:23.500", "evidence": {"kind": "acceptance"}, "action": {"type": "key_sequence", "target": "worksheet.grid", "keys": []}}
        digest = hashlib.sha256(json.dumps(self.action["action"], sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        self.alias = {"ledger": "historical-statements-visual-steps.json", "sourceEventId": "history-0001", "classification": "held_native_state", "reason": "The independently reviewed view remains unchanged after the actual native action.", "actionReferences": [{"id": self.action["id"], "videoTime": self.action["videoTime"], "type": "key_sequence", "actionSha256": digest}], "limitations": ["Preparation is separate from native execution."]}

    def tearDown(self):
        self.temporary.cleanup()

    def join(self, aliases):
        (self.folder / "joined-source-semantic-aliases.json").write_text(json.dumps({"version": 1, "entries": aliases}))
        tree = ast.parse((ROOT / "tools/build_otis_full_manifest.py").read_text())
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "joined_source_coverage")
        namespace = {"Path": Path, "json": json, "hashlib": hashlib, "EVIDENCE": self.folder, "steps": [self.action]}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "joined-source-coverage", "exec"), namespace)
        return namespace["joined_source_coverage"]()

    def test_reviewed_alias_joins_exact_native_chain_and_preserves_qualification(self):
        row = self.join([self.alias])["events"][0]
        self.assertEqual(row["classification"], "held_native_state")
        self.assertEqual(row["actionIds"], ["actual-native-action"])
        self.assertEqual(row["semanticReview"]["limitations"], self.alias["limitations"])

    def test_editorial_boundary_has_explicit_review_without_fake_native_action(self):
        alias = dict(self.alias, classification="editorial-nonapplication-boundary", actionReferences=[], reason="Original end-card footage contains no spreadsheet action.")
        row = self.join([alias])["events"][0]
        self.assertEqual(row["classification"], "editorial-nonapplication-boundary")
        self.assertEqual(row["actionIds"], [])

    def test_unknown_native_id_is_rejected(self):
        alias = dict(self.alias, actionReferences=[dict(self.alias["actionReferences"][0], id="invented-action")])
        with self.assertRaisesRegex(ValueError, "native action"):
            self.join([alias])

    def test_changed_action_semantics_cannot_reuse_a_previously_reviewed_id(self):
        self.action["action"]["keys"] = ["Delete"]
        with self.assertRaisesRegex(ValueError, "semantics"):
            self.join([self.alias])

    def test_duplicate_event_alias_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.join([self.alias, self.alias])

    def test_unknown_source_observation_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "source observation"):
            self.join([dict(self.alias, sourceEventId="history-invented")])

    def test_only_editorial_boundary_can_have_no_native_chain(self):
        with self.assertRaisesRegex(ValueError, "native chain"):
            self.join([dict(self.alias, actionReferences=[])])

    def test_unreviewed_observation_stays_unmapped(self):
        row = self.join([])["events"][0]
        self.assertEqual(row["classification"], "unmapped-source-observation")
        self.assertEqual(row["actionIds"], [])

    def test_dense_intro_source_events_are_loaded_with_native_pts(self):
        ledger = "intro-inspection-native-viewport-boundaries.json"
        event_id = "intro-inspection-native-001"
        (self.folder / ledger).write_text(json.dumps({"observations": [{"id": event_id, "seconds": 21.5, "topRow": 52}]}))
        self.action["videoTime"] = "00:21.500"
        self.action["evidence"] = {"ledger": ledger, "eventId": event_id}
        rows = [row for row in self.join([])["events"] if row["ledger"] == ledger]
        self.assertEqual(len(rows), 1, "Every original native-30fps viewport observation must join the actual action, including newly reviewed intermediate states")
        self.assertEqual(rows[0]["sourceEventId"], event_id)
        self.assertEqual(rows[0]["videoTime"], 21.5, "The dense observation's seconds field retains its actual native source PTS")
        self.assertEqual(rows[0]["actionIds"], [self.action["id"]])
        self.assertEqual(rows[0]["classification"], "native-actions-linked-to-visual-evidence")


if __name__ == "__main__":
    unittest.main()
