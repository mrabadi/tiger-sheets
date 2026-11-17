"""Retain source evidence cited by nested replay qualifications, byte for byte."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("otis_frame_archive", ROOT / "tools/archive_otis_source_frames.py")
ARCHIVE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ARCHIVE)


class RetainedReplayEvidence(unittest.TestCase):
    def test_nested_segment_qualifications_and_review_crop_are_retained(self):
        frames = [Path("/tmp/tigersheets-video-review/row-insert-frames/frame-008.jpg"),
                  Path("/tmp/tigersheets-video-review/income-contact/color-detail.jpg")]
        self.assertTrue(all(path.is_file() for path in frames), "Actual already-reviewed source evidence is available")
        source = {"segments": [{"unresolvedSourceGestures": [{"frames": [str(path) for path in frames]}]}]}
        with tempfile.TemporaryDirectory(dir=ROOT / "output") as temporary:
            base = Path(temporary)
            with patch.object(ARCHIVE, "BASE", base):
                result = ARCHIVE.archive_later_frames(source)
            retained = result["segments"][0]["unresolvedSourceGestures"][0]["frames"]
            self.assertTrue(all(not path.startswith("/tmp/") for path in retained), "Nested source qualifications must not retain ephemeral evidence paths")
            for original, destination in zip(frames, retained):
                self.assertEqual(Path(destination).read_bytes(), original.read_bytes())
            provenance = json.loads(Path(result["retainedLaterFrameProvenance"]).read_text())
            self.assertEqual(len(provenance["frames"]), 2)
            for record in provenance["frames"]:
                self.assertEqual(record["sha256"], hashlib.sha256(Path(record["originalPath"]).read_bytes()).hexdigest())
                self.assertIsNone(record["videoSeconds"], "Supplementary review evidence must not receive an invented timestamp")


if __name__ == "__main__":
    unittest.main()
