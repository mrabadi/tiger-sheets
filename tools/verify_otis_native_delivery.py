#!/usr/bin/env python3
"""Plan or verify the actual saved native model and its portable source pair."""
import argparse
import hashlib
import json
import copy
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.audit_otis_native_final import historical_formula_checks
MODEL = "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"
RAW = "Otis-10-K-Raw.xls"


def build_manifest(model, oracle, refresh=False):
    model = Path(model).resolve()
    cells = {cell["address"]: copy.deepcopy(cell) for cell in oracle["directNativeCheckpoint"]["expect"]["cells"]}
    for cell in historical_formula_checks(oracle) + oracle["exactFormulaChecks"]:
        cells.setdefault(cell["address"], {"address": cell["address"]})["input"] = cell["input"]
    for cell in oracle["independentExactHistoricalValues"]:
        check = cells.setdefault(cell["address"], {"address": cell["address"]})
        check.update(value=cell["value"], valueType="number")
        if cell["value"] != 0:
            check["valueTolerance"] = {"absolute": 1e-8, "relative": 1e-12}
    for column in "DEFGHIJ":
        check = cells.setdefault(column + "100", {"address": column + "100"})
        check.update(value=0, valueType="number")
        check.pop("valueTolerance", None)
    all_cells = list(cells.values())
    steps = []

    def step(identifier, action, expected=None):
        record = {"id": identifier, "videoTime": "postcanonical acceptance",
                  "evidence": {"kind": "acceptance", "method": "Actual native delivery inspection after the tutorial; not an observed source gesture."}, "action": action}
        if expected is not None:
            record["expect"] = expected
        return record

    def keys(identifier, target, values, expected=None):
        steps.append(step(identifier, {"type": "key_sequence", "target": target, "keys": values}, expected))

    def navigate(prefix, address, top_row):
        keys(prefix + "-select-name", "formula.name", ["Ctrl+A"])
        steps.append(step(prefix + "-type-address", {"type": "text", "target": "formula.name", "text": address}))
        keys(prefix + "-navigate", "formula.name", ["Return"])
        steps.append(step(prefix + "-align-row", {"type": "vertical_view", "topRow": top_row}, {"topRow": top_row, "topRowOffset": 0}))
        steps.append(step(prefix + "-align-column", {"type": "horizontal_view", "firstColumn": "A"}, {"firstColumn": 1, "firstColumnOffset": 0}))

    open_children = [step("delivery-open-select-file", {"type": "key_sequence", "target": "fileNameEdit", "keys": ["Ctrl+A"]}),
                     step("delivery-open-file-path", {"type": "text", "target": "fileNameEdit", "text": str(model)}),
                     step("delivery-open-accept", {"type": "key_sequence", "target": "fileNameEdit", "keys": ["Return"]})]
    steps.append(step("delivery-reopen", {"type": "modal", "open": {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+O"]}, "steps": open_children},
                      {"documentName": model.name, "row": 2, "column": 2, "topRow": 1, "firstColumn": 1, "showGridLines": False, "cells": all_cells}))
    if refresh:
        steps.append(step("delivery-data-tab", {"type": "mouse_tab", "target": "workspace.ribbon", "tab": "Data"}, {"ribbon": "Data"}))
        steps.append(step("delivery-refresh-links", {"type": "mouse", "target": "workspace.ribbon", "childAccessibleName": "Refresh links", "gesture": "click"}, {"statusBarMessage": "Refreshed 1 local workbook files", "cells": all_cells}))
        steps.append(step("delivery-home-tab", {"type": "mouse_tab", "target": "workspace.ribbon", "tab": "Home"}, {"ribbon": "Home"}))
    keys("delivery-collapse-ribbon", "worksheet.grid", ["Ctrl+F1"])
    sections = [("drivers", "B2", 1), ("income", "B52", 51), ("balance", "B74", 73), ("cashflow", "B102", 101), ("cashflow-bottom", "B126", 105)]
    for name, address, top_row in sections:
        prefix = "delivery-" + name
        navigate(prefix, address, top_row)
        keys(prefix + "-inspect", "worksheet.grid", [], {"row": int(address[1:]), "column": 2, "topRow": top_row, "topRowOffset": 0, "firstColumn": 1, "showGridLines": False, "cells": all_cells})
    navigate("delivery-restore-drivers", "B2", 1)
    keys("delivery-save-source-final-view", "worksheet.grid", ["Ctrl+S"],
         {"row": 2, "column": 2, "topRow": 1, "topRowOffset": 0, "firstColumn": 1, "firstColumnOffset": 0, "formulaDraft": "Drivers:", "showGridLines": False, "cells": all_cells})
    return {"version": 1, "status": "Prepared, not executed; requires successful canonical replay provenance.", "windowSize": [1280, 720],
            "sourceVideo": "https://www.youtube.com/watch?v=O2uPNfErmJo", "sourceReviewComplete": False, "workspace": str(model.parent),
            "scope": "Actual native reopen, acceptance-only statement inspection and source-view restoration" + (", including actual Data Refresh links and a verified same-folder source-file read." if refresh else "."),
            "inspectionViewQualification": "Only Drivers B2/top1/A is source-certified. Statement topRows51/73/101/105 are explicit acceptance navigation so all statement sections can be inspected at source-size1280x720; they are not claimed as source-observed viewports.",
            "numericTolerance": {"absolute": 1e-8, "relative": 1e-12, "qualification": "Nonzero historical values only; source-exact balance zeros have no tolerance."},
            "steps": steps}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def qualify_canonical(workspace, manifest):
    workspace, manifest = Path(workspace).resolve(), Path(manifest).resolve()
    execution = workspace / "execution"
    try:
        before = json.loads((execution / "run-provenance-before.json").read_text())
        after = json.loads((execution / "run-provenance-after.json").read_text())
        report_path = execution / "action-replay.json"
        report = json.loads(report_path.read_text())
        source_manifest = json.loads(manifest.read_text())
    except (OSError, ValueError) as error:
        raise ValueError("A successful canonical replay is required before delivery verification") from error
    if (after.get("returnCode") != 0 or after.get("executionComplete") is not True
            or after.get("manifestUnchanged") is not True or report.get("executionComplete") is not True
            or report.get("sourceReviewComplete") is not False or source_manifest.get("sourceReviewComplete") is not False
            or after.get("reportSha256") != sha(report_path) or before.get("manifestSha256") != sha(manifest)):
        raise ValueError("A successful canonical replay with unchanged evidence is required")
    delivered = {Path(item["path"]).resolve(): item["sha256"] for item in after.get("deliveredFiles", [])}
    pair = []
    for name in (MODEL, RAW):
        path = workspace / name
        if not path.is_file() or delivered.get(path) != sha(path):
            raise ValueError("Native delivered hash no longer matches canonical evidence: " + name)
        pair.append({"path": str(path), "sha256": sha(path), "bytes": path.stat().st_size})
    return {"qualified": True, "canonicalManifest": str(manifest), "canonicalManifestSha256": sha(manifest),
            "canonicalReport": str(report_path), "canonicalReportSha256": sha(report_path), "pair": pair,
            "sourceReviewComplete": False}


def run_verification(workspace, manifest, binary, output, oracle, native_runner=None, auditor=None):
    qualification = qualify_canonical(workspace, manifest)
    workspace, output = Path(workspace).resolve(), Path(output).resolve()
    binary = Path(binary).resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError("Coherent native replay executable is unavailable")
    if output.exists() and any(output.iterdir()):
        raise ValueError("Delivery verification output exists; preserve the previous evidence")
    output.mkdir(parents=True, exist_ok=True)
    oracle_path = output / "source-oracle-used.json"
    oracle_path.write_text(json.dumps(oracle, indent=2) + "\n")
    summary = {"passed": False, "sourceReviewComplete": False, "startedAtUtc": datetime.now(timezone.utc).isoformat(),
               "canonicalQualification": qualification, "binary": str(binary), "binarySha256": sha(binary), "oracleSha256": sha(oracle_path), "nativePhases": [], "independentAudits": []}
    (output / "delivery-provenance-before.json").write_text(json.dumps(summary, indent=2) + "\n")
    if auditor is None:
        def auditor(model, raw, report):
            environment = dict(os.environ)
            xlrd_path = Path("/tmp/tigersheets-biff-reference")
            if xlrd_path.is_dir():
                environment["PYTHONPATH"] = str(xlrd_path) + os.pathsep + environment.get("PYTHONPATH", "")
            command = [sys.executable, str(ROOT / "tools/audit_otis_native_final.py"), str(model), "--source", str(raw), "--oracle", str(oracle_path), "--output", str(report)]
            with report.with_suffix(".log").open("w") as log:
                process = subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=180)
            result = json.loads(report.read_text())
            if process.returncode != 0:
                result["passed"] = False
            return result
    if native_runner is None:
        def native_runner(native_manifest, trace):
            trace.mkdir(parents=True)
            manifest_path = trace / "manifest.json"
            manifest_path.write_text(json.dumps(native_manifest, indent=2) + "\n")
            screen_path = trace / "screen.json"
            screen_path.write_text(json.dumps({"screens": [{"width": 1280, "height": 800}]}) + "\n")
            environment = dict(os.environ, QT_QPA_PLATFORM="offscreen:configfile=" + str(screen_path))
            command = [str(binary), "--manifest", str(manifest_path), "--output", str(trace), "--acceptance"]
            start = time.monotonic()
            with (trace / "native-replay.log").open("w") as log:
                process = subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=600)
            result = json.loads((trace / "action-replay.json").read_text())
            result["nativeReturnCode"] = process.returncode
            (trace / "native-launch-provenance.json").write_text(json.dumps({"arguments": command, "manifestSha256": sha(manifest_path), "binarySha256": sha(binary), "screen": {"width": 1280, "height": 800}, "returnCode": process.returncode, "elapsedSeconds": time.monotonic() - start}, indent=2) + "\n")
            return result

    def audit(folder, name):
        report_path = output / (name + "-independent-audit.json")
        result = auditor(folder / MODEL, folder / RAW, report_path)
        report_path.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
        summary["independentAudits"].append({"path": str(report_path), "sha256": sha(report_path), "passed": result.get("passed") is True})
        if result.get("passed") is not True:
            raise ValueError("The independent " + name + " audit failed; subsequent native/copy actions were not attempted")

    def phase(folder, name, refresh):
        native_manifest = build_manifest(folder / MODEL, oracle, refresh=refresh)
        manifest_path = output / (name + "-manifest.json")
        manifest_path.write_text(json.dumps(native_manifest, indent=2) + "\n")
        report = native_runner(native_manifest, output / name)
        if report.get("executionComplete") is not True or report.get("sourceReviewComplete") is not False or report.get("nativeReturnCode", 0) != 0:
            raise ValueError("Native " + name + " verification failed; subsequent phases were not attempted")
        summary["nativePhases"].append({"name": name, "manifest": str(manifest_path), "manifestSha256": sha(manifest_path), "executionComplete": True, "sourceReviewComplete": False})

    def pair_hashes(folder):
        return {name: sha(folder / name) for name in (MODEL, RAW)}

    audit(workspace, "canonical-before")
    raw_before = sha(workspace / RAW)
    phase(workspace, "canonical-reopen", True)
    if sha(workspace / RAW) != raw_before:
        raise ValueError("Read-only canonical inspection changed the retained source workbook")
    audit(workspace, "canonical-after-reopen")
    canonical_after = pair_hashes(workspace)
    relocated = output / "relocated-pair"
    relocated.mkdir()
    for name in (MODEL, RAW):
        shutil.copy2(workspace / name, relocated / name)
    if pair_hashes(relocated) != canonical_after:
        raise ValueError("Relocated pair differs from actual natively reopened files before refresh")
    audit(relocated, "relocated-before-refresh")
    phase(relocated, "relocated-refresh", True)
    if sha(relocated / RAW) != raw_before:
        raise ValueError("Refresh changed the retained relocated source workbook")
    audit(relocated, "relocated-after-refresh")
    if pair_hashes(workspace) != canonical_after:
        raise ValueError("Relocated refresh changed the canonical pair")
    summary.update(passed=True, finishedAtUtc=datetime.now(timezone.utc).isoformat(), canonicalPairAfterReopen=canonical_after,
                   relocatedPairAfterRefresh=pair_hashes(relocated), canonicalPairUnchangedDuringRelocatedRefresh=True,
                   qualification="Native reopen/refresh plus independent file checks passed; this does not certify every original application pixel or gesture.")
    (output / "delivery-verification.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


def main():
    evidence = ROOT / "docs/validation/otis-90-minute"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=ROOT / "output/otis-90-minute/native-replay")
    parser.add_argument("--canonical-manifest", type=Path, default=evidence / "full-native-action-manifest.json")
    parser.add_argument("--binary", type=Path, default=ROOT / "build-release/otis_action_replay_tests")
    parser.add_argument("--output", type=Path, default=ROOT / "output/otis-90-minute/native-delivery-verification")
    parser.add_argument("--oracle", type=Path, default=evidence / "final-native-acceptance-oracle.json")
    parser.add_argument("--prepare-directory", type=Path, default=evidence / "native-delivery-preparation")
    parser.add_argument("--execute", action="store_true", help="Only after successful canonical replay: run actual reopen, file audit, pair copy, native link refresh and final audits")
    args = parser.parse_args()
    oracle = json.loads(args.oracle.read_text())
    if args.execute:
        try:
            report = run_verification(args.workspace, args.canonical_manifest, args.binary, args.output, oracle)
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            print(json.dumps({"passed": False, "sourceReviewComplete": False, "error": str(error)}))
            return 2
        print(json.dumps({"passed": report["passed"], "sourceReviewComplete": False, "report": str(args.output.resolve() / "delivery-verification.json")}))
        return 0
    try:
        qualification = qualify_canonical(args.workspace, args.canonical_manifest)
    except ValueError as error:
        qualification = {"qualified": False, "reason": str(error)}
    directory = args.prepare_directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    manifests = []
    for name, model, refresh in [("canonical-reopen", args.workspace / MODEL, True), ("relocated-refresh", args.output / "relocated-pair" / MODEL, True)]:
        path = directory / (name + "-manifest.json")
        native_manifest = build_manifest(model, oracle, refresh=refresh)
        path.write_text(json.dumps(native_manifest, indent=2) + "\n")
        manifests.append({"name": name, "path": str(path), "sha256": sha(path), "topLevelActions": len(native_manifest["steps"])})
    plan = {"preparedOnly": True, "canonicalReady": qualification["qualified"], "canonicalQualification": qualification,
            "sourceReviewComplete": False, "oracle": str(args.oracle.resolve()), "oracleSha256": sha(args.oracle),
            "canonicalWorkspace": str(args.workspace.resolve()), "output": str(args.output.resolve()), "manifests": manifests,
            "binary": str(args.binary.resolve()), "binaryAvailable": args.binary.is_file(),
            "executeCommand": [sys.executable, str(Path(__file__).resolve()), "--workspace", str(args.workspace.resolve()), "--canonical-manifest", str(args.canonical_manifest.resolve()), "--binary", str(args.binary.resolve()), "--output", str(args.output.resolve()), "--oracle", str(args.oracle.resolve()), "--execute"],
            "guards": ["Successful unchanged canonical trace and matching actual delivered pair SHA256 required.", "Existing delivery verification output is never overwritten.", "An independent source-first baseline audit must pass before native save or pair copy.", "Only actual native save changes the model; the source pair is copied byte-for-byte after reopen verification.", "Relocated native Refresh links must preserve the canonical pair and each retained source file.", "Every statement inspection returns to Drivers B2/top1/A before actual Ctrl+S."],
            "limits": ["Prepared manifests are not execution evidence.", "Statement topRows51/73/101/105 are acceptance inspection views, not source-observed final viewports.", "Original Word/Acrobat/Excel chrome adaptations, unresolved source state and other fidelity limitations remain separate.", "User accepted native font substitution and prefers Helvetica by default; explicit source-recorded workbook font names remain unchanged. See user-font-preference.json."]}
    plan_path = directory / "verification-plan.json"
    plan_path.write_text(json.dumps(plan, indent=2) + "\n")
    print(json.dumps({"preparedOnly": True, "canonicalReady": plan["canonicalReady"], "sourceReviewComplete": False, "plan": str(plan_path), "manifests": manifests}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
