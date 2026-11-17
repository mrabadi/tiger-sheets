"""Fail-closed restoration of exact independently reviewed late source records."""
import copy
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DELTA = ROOT / "docs/validation/otis-90-minute/native-history/post-attempt12-late-source-headers/action-delta.json"
DELTA_SHA256 = "eac7e700cf469803ac9e5472358e03483512640d21a8a31002519da27237ad85"


def restore_reviewed_adjacent_source_records(records, segment="balance"):
    """Reverse only the exact published late adjacent-order after-images."""
    # The new complete after-image must be exact before reversing any old layer.
    if segment in ["income", "balance"]:
        retained = ROOT / "docs/validation/otis-90-minute/native-history/post-attempt15-combined-continuation-source-publication-revision"
        previous = retained / "retained-isolated-generation-v3"
        source_pins = {
            "income": [(previous / "income-before.json", "139eef926407f349b55e672523e6120844b5671bcfcd7b22edc3117a9e2d4e3d"),
                       (retained / "retained-isolated-generation-v5/income-after.json", "62338eff4eacea0047705b0025ff63b9a04a3b78e1bd20b4fa4b80c046ea1ac2")],
            "balance": [(previous / "balance-before.json", "2ed242584a48708be87b4d41b0c02a911bd3322d0ddee09a51f625f163cca877"),
                        (retained / "retained-isolated-generation-v6/balance-after.json", "c8681b3753af8be87aaa6446d1d6c3fe0d59c9e34a57d4f540244ed156b2147b")],
        }[segment]
        combined = []
        for path, pin in source_pins:
            assert hashlib.sha256(path.read_bytes()).hexdigest() == pin
            combined.append(json.loads(path.read_text())["steps"])
        assert (len(combined[0]), len(combined[1])) == {"income": (1171, 1166), "balance": (1631, 1654)}[segment]
        assert records == combined[1], "Unapproved action/order: exact reviewed adjacent after-image required"
        records = copy.deepcopy(combined[0])
    folder = ROOT / "docs/validation/otis-90-minute/post-attempt14-late-adjacent-source-consolidation"
    path = folder / "exact-generated-delta-v2.json"
    assert hashlib.sha256(path.read_bytes()).hexdigest() == "94240923ef71a79e4b9350b0b3bb880dea4a04b9d566cdfaf1a9a904745f515a"
    delta = json.loads(path.read_text())[segment]
    pins = {
        "income": ["6e93e9d295083a13f2b5465843a0c473387730e5ff3b78ffa744449eddc86dc0",
                   "139eef926407f349b55e672523e6120844b5671bcfcd7b22edc3117a9e2d4e3d"],
        "balance": ["c96a1fc30792b7e16d92e9db3bca02c07a190b536f0eee58650563a48c63805e",
                    "2ed242584a48708be87b4d41b0c02a911bd3322d0ddee09a51f625f163cca877"],
        "linking": ["8176d46ee7cb1b369549da3d91145080a98102961e379f3e368321670f8c342a",
                    "8176d46ee7cb1b369549da3d91145080a98102961e379f3e368321670f8c342a"],
    }[segment]
    snapshots = {}
    for label, pin in zip(["before", "after-v2"], pins):
        snapshot = folder / (segment + "-" + label + ".json")
        assert hashlib.sha256(snapshot.read_bytes()).hexdigest() == pin
        snapshots[label] = json.loads(snapshot.read_text())
    before = snapshots["before"]["steps"]
    after = snapshots["after-v2"]["steps"]
    assert {k: v for k, v in snapshots["before"].items() if k != "steps"} == {
        k: v for k, v in snapshots["after-v2"].items() if k != "steps"}
    assert (delta["beforeCount"], delta["afterCount"]) == {
        "income": (1169, 1171), "balance": (1627, 1631), "linking": (1098, 1098)}[segment]
    assert (len(delta["changedRecords"]), len(delta["removedRecords"]), len(delta["addedRecords"])) == {
        "income": (4, 0, 2), "balance": (35, 5, 9), "linking": (0, 0, 0)}[segment]
    bm = {r["id"]: r for r in before}
    am = {r["id"]: r for r in after}
    assert len(bm) == len(before) and len(am) == len(after)
    changed = {r["id"] for r in delta["changedRecords"]}
    removed = {r["id"] for r in delta["removedRecords"]}
    added = {r["id"] for r in delta["addedRecords"]}
    assert changed == {k for k in bm if k in am and bm[k] != am[k]}
    assert removed == set(bm) - set(am) and added == set(am) - set(bm)
    for r in delta["changedRecords"]:
        assert r == {"id": r["id"], "before": bm[r["id"]], "after": am[r["id"]]}
    for r in delta["removedRecords"]:
        assert r == bm[r["id"]]
    for r in delta["addedRecords"]:
        assert r == am[r["id"]]
    assert [r for r in before if r["id"] not in changed | removed] == [
        r for r in after if r["id"] not in changed | added]
    assert records == after, "Unapproved action/order: exact reviewed adjacent after-image required"
    return copy.deepcopy(before)


def restore_reviewed_late_source_records(records, segment="balance"):
    records = restore_reviewed_adjacent_source_records(records, segment)
    assert hashlib.sha256(DELTA.read_bytes()).hexdigest() == DELTA_SHA256
    proof = json.loads(DELTA.read_text())["segments"][segment]
    baseline = Path(proof["beforePath"])
    assert hashlib.sha256(baseline.read_bytes()).hexdigest() == proof["beforeSha256"]
    current = {step["id"]: step for step in records}
    assert len(current) == len(records)
    assert [step["id"] for step in records] == proof["afterOrder"], "Unapproved record order or membership"
    restored = copy.deepcopy(current)
    for change in proof["changedRecords"]:
        assert current[change["id"]] == change["after"], change["id"]
        restored[change["id"]] = copy.deepcopy(change["before"])
    for added in proof["addedRecords"]:
        assert current[added["id"]] == added, added["id"]
        del restored[added["id"]]
    for removed in proof["removedRecords"]:
        assert removed["id"] not in current
        restored[removed["id"]] = copy.deepcopy(removed)
    assert set(restored) == set(proof["beforeOrder"])
    return [restored[identifier] for identifier in proof["beforeOrder"]]
