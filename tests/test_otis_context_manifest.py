import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"


class SourceContextManifest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result = subprocess.run([sys.executable, str(ROOT / "tools/build_otis_context_manifest.py")], capture_output=True, text=True, timeout=90)
        if result.returncode:
            raise RuntimeError(result.stderr)
        cls.manifest = json.loads((BASE / "external-context-native-action-manifest.json").read_text())

    def events(self, *identifiers):
        return [step for step in self.manifest["steps"] if step["evidence"]["eventId"] in identifiers]

    def test_source_slides_fill_the_original_1280_by_720_presentation_canvas(self):
        opening = self.events("early-intro-context-001")
        self.assertTrue(any(step["action"] == {"type": "key_sequence", "target": "@context_window", "keys": ["F5"]} and step.get("expect", {}).get("externalDocument") == {"page": 1, "zoomPercent": 100, "presentationView": True} for step in opening), "The source full-size slide cannot be replayed as a75%960x540 image surrounded by reader controls")
        masks = [step["action"]["masks"] for step in self.manifest["steps"] if step["action"].get("type") == "presentation_builds" and step["action"].get("operation") == "set"]
        self.assertTrue(masks)
        self.assertTrue(all(rectangle[2] == 1280 for stages in masks for stage in stages for rectangle in stage), "Every reveal mask must scale with the genuine full-size PDF canvas")
        page2 = next(step["action"]["masks"] for step in self.manifest["steps"] if "configure-slide-2" in step["id"])
        self.assertEqual([stage[0][1] for stage in page2[:-1]], [100, 472], "The independently verified PDF-point paragraph bands must convert to96dpi, leaving the title and footer intact")
        annotation = next(step["action"] for step in self.events("early-intro-context-028") if step["action"].get("operation") == "press")
        self.assertEqual(annotation["point"], [208, 306], "The independently measured affine maps the actual slide3 anchor into the full96dpi raster")

    def test_ambiguous_acrobat_word_research_tag_retains_actual_current_pdf(self):
        debt = self.events(*["linking-debt-checks-" + str(number) for number in range(321, 332)])
        opened = [Path(step["action"]["path"]).name for step in debt if step["action"]["type"] == "document_context" and step["action"].get("operation") == "open"]
        self.assertEqual(opened, ["101-01-Otis-10-K.pdf"], "Debt research must stay in the10-K; a generic app label must not switch to the case-study PDF")
        outlook = self.events("linking-debt-checks-418", "linking-debt-checks-419", "linking-debt-checks-420")
        self.assertFalse(any(step["action"].get("path", "").endswith("Case-Study.pdf") for step in outlook), "Investor page42 navigation must retain the investor presentation")
        page = [step for step in self.events("linking-debt-checks-419") if step["action"].get("target") == "context.page" and step["action"].get("type") == "text"]
        self.assertEqual([step["action"]["text"] for step in page], ["42"])

    def test_debt_results_and_page_field_focus_are_actual_native_states(self):
        first = self.events("linking-debt-checks-325")
        self.assertTrue(first, "The source first debt result needs a read-only native state assertion")
        self.assertEqual(first[-1]["expect"]["externalDocument"], {"page": 16, "query": "long-term debt", "matchIndex": 1, "matchCount": 40})
        second = self.events("linking-debt-checks-326")
        self.assertEqual(second[-1]["action"], {"type": "key_sequence", "target": "context.find", "keys": ["Return"]})
        self.assertEqual(second[-1]["expect"]["externalDocument"], {"page": 38, "query": "long-term debt", "matchIndex": 2, "matchCount": 40})
        self.assertIn("39", second[-1]["evidence"]["executionMethod"])
        focus = self.events("linking-debt-checks-331")
        self.assertEqual(focus[-1]["action"], {"type": "mouse", "target": "context.page", "gesture": "click"})
        self.assertEqual(focus[-1]["expect"]["externalDocument"]["page"], 38)

    def test_edit_pdf_tooltip_carry_clear_and_growth_suggestions_have_real_native_actions(self):
        first = self.events("early-intro-context-078")
        self.assertTrue(first, "The observed Edit PDF hover cannot be silently omitted")
        self.assertEqual(first[-1]["action"], {"type": "mouse", "target": "context.editPdf", "gesture": "hover", "dwellMs": 1000})
        self.assertEqual(first[-1]["expect"]["externalDocument"], {"page": 60, "editPdfToolTipVisible": True, "toolTipText": "Edit PDF"})
        carry = self.events("early-intro-context-079")[-1]
        self.assertEqual(carry["expect"]["externalDocument"], {"page": 6, "editPdfToolTipVisible": True, "toolTipText": "Edit PDF"})
        clear = self.events("early-intro-context-080")[-1]
        self.assertEqual(clear["action"]["target"], "context.surface")
        self.assertEqual(clear["expect"]["externalDocument"], {"page": 6, "editPdfToolTipVisible": False, "toolTipText": ""})
        suggestions = self.events("income-091")
        self.assertEqual(suggestions[0]["action"], {"type": "mouse", "target": "context.find", "gesture": "click"})
        self.assertEqual(suggestions[0]["expect"]["externalDocument"], {"page": 11, "query": "growth", "findSuggestionsVisible": True})
        self.assertEqual(suggestions[-1]["videoTime"], "40:09")
        self.assertEqual(suggestions[-1]["action"], {"type": "key_sequence", "target": "context.find", "keys": ["Escape"]})
        self.assertIs(suggestions[-1]["expect"]["externalDocument"]["findSuggestionsVisible"], False)
        self.assertIn("cloud", suggestions[0]["evidence"]["executionMethod"])


class SourceWorkbookTabAnnotation(unittest.TestCase):
    def test_observed_operations_tab_held_resize_and_clear_are_real_native_actions(self):
        process = subprocess.run([sys.executable, str(ROOT / "tools/build_otis_model_annotation_manifest.py")], capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        manifest = json.loads((BASE / "model-annotation-native-action-manifest.json").read_text())
        identifiers = ["early-intro-context-182", "early-intro-context-183", "early-intro-context-184"]
        actions = [step for step in manifest["steps"] if step["evidence"]["eventId"] in identifiers]
        self.assertEqual([step["action"]["operation"] for step in actions], ["press", "move", "move", "release", "clear"], "The actual source tab rectangle must include both held sizes and its later clear")
        self.assertEqual([step["videoTime"] for step in actions], ["07:00.500", "07:00.500", "07:01.000", "07:01.000", "07:02.500"])
        for step in actions:
            self.assertEqual(step["action"]["type"], "annotation")
            self.assertEqual(step["action"]["target"], "workspace.sheets")
            self.assertEqual(step["action"]["tabName"], "operations")
            self.assertIn("actual native tab", step["evidence"]["executionMethod"])
        self.assertLess(actions[0]["action"]["tabPoint"][1], 0)
        self.assertGreater(actions[2]["action"]["tabPoint"][1], actions[1]["action"]["tabPoint"][1])
        self.assertTrue(set(identifiers).issubset(manifest["coveredSourceEvents"]))


if __name__ == "__main__":
    unittest.main()
