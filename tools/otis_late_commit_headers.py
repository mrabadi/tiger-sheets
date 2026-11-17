"""Apply independently reviewed Ready-header source counterparts to late replays."""
import json
import re
from copy import deepcopy


def apply_late_commit_headers(steps, evidence_root):
    path = evidence_root / "late-ready-commit-header-phases.json"
    phases = json.loads(path.read_text())
    by_id = {step["id"]: step for step in steps}
    base_evidence = {step["id"]: deepcopy(step["evidence"]) for step in steps}
    changes = [(by_id[row["id"]], row) for row in phases["records"] if row["id"] in by_id]
    adjacent = [(by_id[row["id"]], row) for row in phases.get("adjacentChanges", []) if row["id"] in by_id]
    additions = [row for row in phases.get("addedRecords", []) if row["anchorEvidenceFrom"] in by_id]
    removals = [row for row in phases.get("removedRecords", []) if row["id"] in by_id]
    for step, row in changes:
        assert step["action"] == row["expectedAction"], (row["id"], step["action"])
        assert row["keys"] in [["Ctrl+Return"], ["Tab"]], row["id"]
        assert re.fullmatch(r"[A-Z]+[1-9][0-9]*", row["sourceNameBox"]), row["id"]
    for step, row in adjacent:
        assert step["action"] == row["expectedAction"], (row["id"], step["action"])
    for row in removals:
        assert by_id[row["id"]]["action"] == row["expectedAction"], row["id"]
    for row in additions:
        assert row["id"] not in by_id, row["id"]
        assert base_evidence[row["anchorEvidenceFrom"]]["eventId"] == row["eventId"], row["id"]

    def evidence_for(original, row):
        evidence = deepcopy(original)
        evidence["sourcePhaseLedger"] = path.name
        evidence["frames"] = [str((evidence_root / frame).resolve()) for frame in row["frames"]]
        evidence["executionMethod"] = " ".join(filter(None, [evidence.get("executionMethod"), phases["qualification"], row.get("sourceTimeQualification")]))
        if "observed" in row:
            evidence["observed"] = row["observed"]
        if "sourceEventId" in row:
            evidence["eventId"] = row["sourceEventId"]
        return evidence
    for step, row in changes:
        address = row["sourceNameBox"]
        column = 0
        for letter in re.match(r"[A-Z]+", address)[0]:
            column = column * 26 + ord(letter) - ord("A") + 1
        step["action"]["keys"] = row["keys"]
        step.setdefault("expect", {}).update(nameBoxText=address, row=int(re.search(r"\d+", address)[0]), column=column)
        if "videoTime" in row:
            step["videoTime"] = row["videoTime"]
        step["evidence"] = evidence_for(base_evidence[row["id"]], row)
        step["evidence"]["observedReadyHeader"] = {"status": "Ready", "nameBox": address, "formulaBar": row.get("sourceFormulaBar")}
    for step, row in adjacent:
        if "action" in row:
            step["action"] = deepcopy(row["action"])
        step["videoTime"] = row["videoTime"]
        step.setdefault("expect", {}).update(row.get("expect", {}))
        if not step["expect"]:
            step.pop("expect")
        step["evidence"] = evidence_for(base_evidence[row["id"]], row)
    removed_ids = {row["id"] for row in removals}
    steps[:] = [step for step in steps if step["id"] not in removed_ids]
    for row in additions:
        steps.append({"id": row["id"], "videoTime": row["videoTime"],
                      "evidence": evidence_for(base_evidence[row["anchorEvidenceFrom"]], row),
                      "action": deepcopy(row["action"]), "expect": deepcopy(row["expect"])})
