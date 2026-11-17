"""Source dimension headers remain visible inside the actual Paste Special modal."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ("initial-050", "1R x 3C", "C2"),
    ("history-0201", "2R x 3C", "C8"),
    ("history-0240", "2R x 1C", "C14"),
    ("history-0602", "6R x 1C", "C27"),
    ("history-0639", "1R x 2C", "D28"),
    ("history-0753", "3R x 2C", "D36"),
]


class HistoricalModalHeaderPhases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="otis-modal-header-test-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.manifests = {}
        for intro in [False, True]:
            folder = Path(cls.directory.name) / str(intro)
            target = folder / "manifest.json"
            command = [sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"),
                       "--workspace", str(folder / "work"), "--output", str(target)]
            if intro:
                command.append("--include-intro")
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            cls.manifests[intro] = json.loads(target.read_text())

    def parent(self, manifest, event):
        parents = [step for step in manifest["steps"]
                   if step["evidence"].get("eventId") == event and step["action"]["type"] == "modal"]
        self.assertEqual(len(parents), 1)
        return parents[0]

    def test_every_observed_modal_count_is_checked_on_its_real_child(self):
        for intro, manifest in self.manifests.items():
            for event, count, _ in CASES:
                with self.subTest(intro=intro, event=event):
                    modal = self.parent(manifest, event)
                    guarded = [child for child in modal["action"]["steps"]
                               if child.get("expect", {}).get("nameBoxText") == count]
                    self.assertTrue(guarded, "The original source modal retains " + count)
                    self.assertTrue(all(child["action"].get("target", "").startswith("pasteSpecial.") for child in guarded),
                                    "Inspect the actual modal without setting Name Box artwork")

    def test_real_modifier_release_happens_inside_the_modal_before_acceptance(self):
        for intro, manifest in self.manifests.items():
            for event, count, anchor in CASES:
                with self.subTest(intro=intro, event=event):
                    modal = self.parent(manifest, event)
                    children = modal["action"]["steps"]
                    release = [child for child in children if child["action"]["type"] == "modifier_release"]
                    self.assertEqual(len(release), 1, "Do not release the native dimension header before opening its source modal")
                    self.assertEqual(release[0]["action"], {"type": "modifier_release", "target": "pasteSpecial.dialog", "keys": ["Shift"]})
                    self.assertEqual(release[0]["expect"]["nameBoxText"], anchor)
                    accepted = next(child for child in children if child["action"].get("keys") == ["Return"])
                    guarded = next(child for child in children if child.get("expect", {}).get("nameBoxText") == count)
                    self.assertLess(children.index(guarded), children.index(release[0]))
                    self.assertLess(children.index(release[0]), children.index(accepted))

    def test_existing_held_net_income_modal_control_is_preserved(self):
        for manifest in self.manifests.values():
            modal = self.parent(manifest, "history-0302")
            self.assertTrue(any(child.get("expect", {}).get("nameBoxText") == "3R x 1C"
                                for child in modal["action"]["steps"]))


if __name__ == "__main__":
    unittest.main()
