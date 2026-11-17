#!/usr/bin/env python3
"""Guard and record a real native replay; never construct worksheet data."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
INPUTS = {
    "Otis-10-K-Raw.xls": "08cabfcfd01213bc588176387be36b7e3610267c9633987e90b404fc0ed492f4",
    "101-01-90-Minute-Case-Study-Otis.xlsx": "7e7b7967312980c8e57772873764810a2df47ebe77264f1e6de2168d9562412e",
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(manifest_path, binary_path, output_path):
    manifest_path, binary_path, output_path = (Path(path).resolve() for path in (manifest_path, binary_path, output_path))
    manifest = json.loads(manifest_path.read_text())
    workspace = Path(manifest["workspace"]).resolve()
    if (workspace / "101-01-90-Minute-Case-Study-Otis-Blank.xlsx").exists():
        raise ValueError("Native model already exists; preserve this run and prepare a fresh workspace")
    if output_path.exists() and any(output_path.iterdir()):
        raise ValueError("Native trace output already exists; no replay evidence will be overwritten")
    inputs = []
    for name, expected in INPUTS.items():
        path = workspace / name
        if not path.is_file() or sha(path) != expected:
            raise ValueError("Expected untouched official input: " + name)
        inputs.append({"path": str(path), "sha256": expected, "bytes": path.stat().st_size})
    if manifest.get("sourceReviewComplete") is not False:
        raise ValueError("This initial observed-step replay must preserve sourceReviewComplete:false")
    if not binary_path.is_file() or not os.access(binary_path, os.X_OK):
        raise ValueError("The coherent native replay executable is unavailable")
    return {
        "manifest": str(manifest_path), "manifestSha256": sha(manifest_path),
        "binary": str(binary_path), "binarySha256": sha(binary_path),
        "workspace": str(workspace), "output": str(output_path), "inputFilesBefore": inputs,
        "sourceReviewComplete": False,
        "screen": {"screens": [{"width": 1280, "height": 800}]},
        "environment": {"QT_QPA_PLATFORM": "offscreen:configfile=" + str(output_path / "screen.json")},
        "arguments": [str(binary_path), "--manifest", str(manifest_path), "--output", str(output_path), "--acceptance"],
        "meaning": "Only successful execution proves native actions ran. --acceptance does not change the source-review flag or claim pixel identity.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "docs/validation/otis-90-minute/full-native-action-manifest.json")
    parser.add_argument("--binary", type=Path, default=ROOT / "build-release/otis_action_replay_tests")
    parser.add_argument("--output", type=Path, default=ROOT / "output/otis-90-minute/native-replay/execution")
    parser.add_argument("--plan-out", type=Path)
    parser.add_argument("--execute", action="store_true", help="Run the actual native Qt replay after all guards pass")
    parser.add_argument("--timeout", type=int, default=7200)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 86400:
        parser.error("timeout must be between 1 and 86400 seconds")
    plan = prepare(args.manifest, args.binary, args.output)
    if args.plan_out:
        args.plan_out.parent.mkdir(parents=True, exist_ok=True)
        args.plan_out.write_text(json.dumps(plan, indent=2) + "\n")
    if not args.execute:
        print(json.dumps(plan, indent=2))
        return 0
    output = Path(plan["output"])
    output.mkdir(parents=True, exist_ok=True)
    (output / "screen.json").write_text(json.dumps(plan["screen"]) + "\n")
    plan["startedAtUtc"] = datetime.now(timezone.utc).isoformat()
    (output / "run-provenance-before.json").write_text(json.dumps(plan, indent=2) + "\n")
    started = time.monotonic()
    environment = dict(os.environ, **plan["environment"])
    with (output / "native-replay.log").open("w") as log:
        try:
            result = subprocess.run(plan["arguments"], cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
    after = {"returnCode": code, "elapsedSeconds": time.monotonic() - started, "manifestUnchanged": sha(Path(plan["manifest"])) == plan["manifestSha256"]}
    report = output / "action-replay.json"
    if report.is_file():
        native = json.loads(report.read_text())
        after.update(reportSha256=sha(report), executionComplete=native.get("executionComplete"), sourceReviewComplete=native.get("sourceReviewComplete"), complete=native.get("complete"))
    after["deliveredFiles"] = [{"path": str(path), "sha256": sha(path), "bytes": path.stat().st_size} for name in [*INPUTS, "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"] if (path := Path(plan["workspace"]) / name).is_file()]
    (output / "run-provenance-after.json").write_text(json.dumps(after, indent=2) + "\n")
    print(json.dumps(after, indent=2))
    return code if code else 0 if after.get("executionComplete") and after["manifestUnchanged"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
