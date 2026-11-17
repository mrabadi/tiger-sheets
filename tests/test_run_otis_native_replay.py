import hashlib
import json
from pathlib import Path
import runpy
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
prepare = runpy.run_path(str(ROOT / "tools/run_otis_native_replay.py"))["prepare"]
REFERENCE = ROOT / "docs/validation/otis-90-minute/reference"


class NativeReplayPreflight(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.workspace = Path(self.temp.name)
        for target, source in [("Otis-10-K-Raw.xls", "101-01-Otis-10-K-Raw.xls"), ("101-01-90-Minute-Case-Study-Otis.xlsx", "101-01-90-Minute-Case-Study-Otis-Complete.xlsx")]:
            (self.workspace / target).write_bytes((REFERENCE / source).read_bytes())
        self.manifest = self.workspace / "manifest.json"
        self.manifest.write_text(json.dumps({"workspace": str(self.workspace), "sourceReviewComplete": False, "steps": []}))
        self.output = self.workspace / "execution"
        self.binary = ROOT / "build-release/otis_action_replay_tests"

    def tearDown(self):
        self.temp.cleanup()

    def plan(self):
        return prepare(self.manifest, self.binary, self.output)

    def test_modified_raw_is_rejected_before_any_output_or_native_execution(self):
        (self.workspace / "Otis-10-K-Raw.xls").write_bytes(b"edited source")
        with self.assertRaisesRegex(ValueError, "untouched.*Raw"):
            self.plan()
        self.assertFalse(self.output.exists())

    def test_modified_completed_demo_is_also_rejected(self):
        (self.workspace / "101-01-90-Minute-Case-Study-Otis.xlsx").write_bytes(b"edited demo")
        with self.assertRaisesRegex(ValueError, "untouched.*Otis.xlsx"):
            self.plan()
        self.assertFalse(self.output.exists())

    def test_existing_native_model_and_trace_are_never_overwritten(self):
        model = self.workspace / "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"
        model.write_bytes(b"keep native work")
        with self.assertRaisesRegex(ValueError, "model already exists"):
            self.plan()
        self.assertEqual(model.read_bytes(), b"keep native work")
        model.unlink()
        self.output.mkdir()
        (self.output / "action-trace.jsonl").write_text("keep trace")
        with self.assertRaisesRegex(ValueError, "trace output"):
            self.plan()

    def test_prepared_command_preserves_false_review_flag_and_explicit_screen(self):
        plan = self.plan()
        self.assertIs(plan["sourceReviewComplete"], False)
        self.assertEqual(plan["screen"], {"screens": [{"width": 1280, "height": 800}]})
        self.assertEqual(plan["arguments"][-1], "--acceptance")
        self.assertEqual(plan["environment"]["QT_QPA_PLATFORM"], "offscreen:configfile=" + str(self.output / "screen.json"))
        self.assertEqual(plan["manifestSha256"], hashlib.sha256(self.manifest.read_bytes()).hexdigest())
        self.assertFalse(self.output.exists(), "Preparing a command must not launch the application or touch worksheet files")


if __name__ == "__main__":
    unittest.main()
