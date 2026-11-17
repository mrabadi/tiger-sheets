import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = runpy.run_path(str(ROOT / "tools/verify_otis_native_delivery.py"))
MODEL = "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"
RAW = "Otis-10-K-Raw.xls"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CanonicalQualification(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.workspace = Path(self.temp.name)
        self.execution = self.workspace / "execution"
        self.execution.mkdir()
        self.manifest = self.workspace / "full-manifest.json"
        self.manifest.write_text(json.dumps({"sourceReviewComplete": False, "steps": []}))
        for name in (MODEL, RAW):
            (self.workspace / name).write_bytes(("synthetic guard fixture " + name).encode())
        self.report = {"executionComplete": True, "sourceReviewComplete": False, "complete": False}
        self.after = {"returnCode": 0, "executionComplete": True, "manifestUnchanged": True,
                      "reportSha256": "", "deliveredFiles": [{"path": str(self.workspace / name), "sha256": digest(self.workspace / name)} for name in (MODEL, RAW)]}
        self.before = {"manifestSha256": digest(self.manifest)}
        self.write_provenance()

    def tearDown(self):
        self.temp.cleanup()

    def write_provenance(self):
        report = self.execution / "action-replay.json"
        report.write_text(json.dumps(self.report))
        self.after["reportSha256"] = digest(report)
        (self.execution / "run-provenance-after.json").write_text(json.dumps(self.after))
        (self.execution / "run-provenance-before.json").write_text(json.dumps(self.before))

    def test_incomplete_or_changed_canonical_evidence_is_refused_without_copying(self):
        for field, value in [("returnCode", 2), ("executionComplete", False), ("manifestUnchanged", False)]:
            with self.subTest(field=field):
                previous = self.after[field]
                self.after[field] = value
                self.write_provenance()
                with self.assertRaisesRegex(ValueError, "successful canonical"):
                    MODULE["qualify_canonical"](self.workspace, self.manifest)
                self.after[field] = previous
        self.write_provenance()
        (self.workspace / RAW).write_bytes(b"source changed after replay")
        with self.assertRaisesRegex(ValueError, "delivered hash"):
            MODULE["qualify_canonical"](self.workspace, self.manifest)
        self.assertEqual(sorted(path.name for path in self.workspace.iterdir()), [MODEL, RAW, "execution", "full-manifest.json"])


class DeliveryManifest(unittest.TestCase):
    def test_all_source_mapped_historical_formulas_are_checked_during_native_inspection(self):
        oracle = json.loads((ROOT / "docs/validation/otis-90-minute/final-native-acceptance-oracle.json").read_text())
        manifest = MODULE["build_manifest"](Path("/tmp/pair") / MODEL, oracle, refresh=True)
        for step in manifest["steps"]:
            if "cells" not in step.get("expect", {}):
                continue
            checks = {item["address"]: item for item in step["expect"]["cells"]}
            self.assertTrue(all("input" in checks[item["address"]] for item in oracle["independentExactHistoricalValues"]), "A native value-only check must not waive the exact source-mapped historical formula")
            self.assertEqual(checks["C54"]["input"], "='[Otis-10-K-Raw.xls]operations'!G8")
            self.assertEqual(checks["E126"]["input"], "=E109+E114+E122+E124")

    def test_actual_reopen_and_refresh_are_read_only_until_view_restoring_save(self):
        oracle = json.loads((ROOT / "docs/validation/otis-90-minute/final-native-acceptance-oracle.json").read_text())
        manifest = MODULE["build_manifest"](Path("/tmp/relocated pair") / MODEL, oracle, refresh=True)
        self.assertEqual(manifest.get("windowSize"), [1280, 720], "Inspection must execute in source-size native windows")
        self.assertIs(manifest["sourceReviewComplete"], False)
        steps = manifest["steps"]
        self.assertEqual(steps[0]["action"]["type"], "modal")
        self.assertEqual(steps[0]["action"]["open"]["keys"], ["Ctrl+O"])
        self.assertEqual(steps[0]["action"]["steps"][1]["action"]["text"], "/tmp/relocated pair/" + MODEL)
        refresh = next(step for step in steps if step["action"].get("childAccessibleName") == "Refresh links")
        self.assertEqual(refresh["action"]["type"], "mouse")
        self.assertEqual(refresh["action"]["target"], "workspace.ribbon")
        formula_checks = {cell["address"]: cell["input"] for cell in refresh["expect"]["cells"] if "input" in cell}
        self.assertTrue(all(formula_checks[cell["address"]] == cell["input"] for cell in oracle["exactFormulaChecks"]))
        values = {cell["address"]: cell for cell in refresh["expect"]["cells"] if "value" in cell}
        for cell in oracle["independentExactHistoricalValues"]:
            self.assertEqual(values[cell["address"]]["value"], cell["value"])
            self.assertEqual(values[cell["address"]]["valueType"], "number")
        for column in "DEFGHIJ":
            self.assertEqual(values[column + "100"]["value"], 0)
            self.assertNotIn("valueTolerance", values[column + "100"])
        self.assertTrue(all(step["evidence"]["kind"] == "acceptance" for step in steps))
        views = [step for step in steps if step["id"].endswith("-inspect")]
        self.assertEqual([step["expect"]["topRow"] for step in views], [1, 51, 73, 101, 105])
        final = steps[-1]
        self.assertEqual(final["action"]["keys"], ["Ctrl+S"])
        self.assertEqual({key: final["expect"][key] for key in ("row", "column", "topRow", "firstColumn", "formulaDraft")},
                         {"row": 2, "column": 2, "topRow": 1, "firstColumn": 1, "formulaDraft": "Drivers:"})
        for step in steps:
            if step["action"]["type"] == "text":
                self.assertEqual(step["action"]["target"], "formula.name", "Inspection may navigate but never replace a worksheet input")

    def test_plan_only_cli_writes_qualified_templates_without_sources_or_native_binary(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            preparation, workspace, output = base / "prepared", base / "absent-canonical", base / "untouched-output"
            result = subprocess.run([sys.executable, str(ROOT / "tools/verify_otis_native_delivery.py"), "--workspace", str(workspace), "--output", str(output), "--prepare-directory", str(preparation), "--binary", str(base / "absent-native")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((preparation / "verification-plan.json").exists(), "Plan-only must actually prepare the inspectable delivery verification")
            plan = json.loads((preparation / "verification-plan.json").read_text())
            self.assertIs(plan["preparedOnly"], True)
            self.assertIs(plan["canonicalReady"], False)
            self.assertEqual(len(plan["manifests"]), 2)
            self.assertFalse(workspace.exists())
            self.assertFalse(output.exists(), "Preparing must not create a copied pair, native trace or worksheet file")
            for item in plan["manifests"]:
                self.assertIs(json.loads(Path(item["path"]).read_text())["sourceReviewComplete"], False)


class DeliveryExecution(CanonicalQualification):
    def test_original_and_relocated_delivery_require_an_actual_source_file_refresh(self):
        oracle = json.loads((ROOT / "docs/validation/otis-90-minute/final-native-acceptance-oracle.json").read_text())
        observed = []
        def native(manifest, trace):
            refresh = [step for step in manifest["steps"] if step["action"].get("childAccessibleName") == "Refresh links"]
            self.assertEqual(len(refresh), 1, "Both original and copied pairs must execute the genuine Refresh links command")
            self.assertEqual(refresh[0]["expect"].get("statusBarMessage"), "Refreshed 1 local workbook files", "Unchanged model values alone cannot distinguish a source-file read from a cache fallback")
            observed.append(manifest["workspace"])
            return {"executionComplete": True, "sourceReviewComplete": False, "nativeReturnCode": 0}
        result = MODULE["run_verification"](self.workspace, self.manifest, Path("/bin/true"), self.workspace / "delivery-check", oracle, native_runner=native, auditor=lambda model, raw, report: {"passed": True})
        self.assertTrue(result["passed"])
        self.assertEqual(len(observed), 2)

    def test_failed_independent_baseline_audit_prevents_native_save_or_pair_copy(self):
        calls = []
        output = self.workspace / "delivery-check"
        def audit(model, raw, report):
            calls.append(("audit", Path(model).parent))
            return {"passed": False, "model": {"failures": [{"code": "meaningful independent discrepancy"}]}}
        def native(manifest, trace):
            calls.append(("native", trace))
            return {"executionComplete": True}
        with self.assertRaisesRegex(ValueError, "independent.*audit"):
            MODULE["run_verification"](self.workspace, self.manifest, Path("/bin/true"), output, {}, native_runner=native, auditor=audit)
        self.assertEqual(calls, [("audit", self.workspace)])
        self.assertFalse((output / "relocated-pair").exists())

    def test_copy_uses_native_reopened_bytes_then_real_refresh_boundary_and_keeps_original_pair(self):
        calls = []
        output = self.workspace / "delivery-check"
        original_raw = (self.workspace / RAW).read_bytes()
        oracle = json.loads((ROOT / "docs/validation/otis-90-minute/final-native-acceptance-oracle.json").read_text())
        def audit(model, raw, report):
            calls.append(("audit", Path(model).parent))
            self.assertEqual(Path(raw).read_bytes(), original_raw)
            return {"passed": True}
        def native(manifest, trace):
            folder = Path(manifest["workspace"])
            calls.append(("native", folder))
            if folder == self.workspace:
                self.assertFalse((output / "relocated-pair").exists())
                (folder / MODEL).write_bytes(b"synthetic bytes saved by native-boundary stub")
            else:
                self.assertEqual((folder / MODEL).read_bytes(), (self.workspace / MODEL).read_bytes())
                self.assertTrue(any(step["action"].get("childAccessibleName") == "Refresh links" for step in manifest["steps"]))
            return {"executionComplete": True, "sourceReviewComplete": False}
        report = MODULE["run_verification"](self.workspace, self.manifest, Path("/bin/true"), output, oracle, native_runner=native, auditor=audit)
        relocated = output / "relocated-pair"
        self.assertEqual(calls, [("audit", self.workspace), ("native", self.workspace), ("audit", self.workspace), ("audit", relocated), ("native", relocated), ("audit", relocated)])
        self.assertIs(report.get("passed"), True, "Only both successful native phases and all independent audits qualify delivery")
        self.assertIs(report["sourceReviewComplete"], False)
        self.assertEqual((self.workspace / RAW).read_bytes(), original_raw)
        self.assertEqual((relocated / RAW).read_bytes(), original_raw)
        self.assertEqual((relocated / MODEL).read_bytes(), (self.workspace / MODEL).read_bytes())
        self.assertTrue(report["canonicalPairUnchangedDuringRelocatedRefresh"])


if __name__ == "__main__":
    unittest.main()
