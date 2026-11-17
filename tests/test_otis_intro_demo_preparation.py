"""Source-observed intro variant contracts, independent of published formatting."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOLLAR_ACCOUNTING = r'_("$"* #,##0_);_("$"* \(#,##0\);_("$"* "-"_);_(@_)'
NONE_ACCOUNTING = r'_(* #,##0_);_(* \(#,##0\);_(* "-"_);_(@_)'
PLAIN_DRIVERS = ["F15", "F16", "F17", "F19", "F20", "F22", "F23", "F24", "F26", "F27", "F28", "F30", "F32", "F33", "F34", "F36", "F46", "F48", "F50", "F51"]


class SourceIntroDemoPreparationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.work = Path(cls.folder.name) / "work"
        output = Path(cls.folder.name) / "intro.json"
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(cls.work), "--include-intro", "--through", "26", "--output", str(output)], check=True, capture_output=True)
        cls.manifest = json.loads(output.read_text())
        cls.records = []
        def visit(records):
            for record in records:
                cls.records.append(record)
                visit(record["action"].get("steps", []))
        visit(cls.manifest["steps"])
        cls.checks = [cell for step in cls.records for cell in step.get("expect", {}).get("cells", [])]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_all_native_30fps_intro_viewport_changes_have_native_navigation(self):
        boundary = json.loads((ROOT / "docs/validation/otis-90-minute/intro-inspection-native-viewport-boundaries.json").read_text())
        views = [step for step in self.records if step["id"].endswith("intro-inspection-viewport")]
        actual = {(step["videoTime"], step["action"].get("topRow")): step for step in views}
        for source in boundary["observations"]:
            seconds = source["seconds"]
            time = f"{int(seconds//60):02d}:{seconds%60:06.3f}"
            with self.subTest(time=time, row=source["topRow"]):
                self.assertIn((time, source["topRow"]), actual, "Every source-visible viewport intermediate needs real native wheel navigation")
                step = actual[(time, source["topRow"])]
                self.assertEqual(step["action"]["type"], "vertical_view")
                self.assertEqual(step["expect"]["row"], 53)
                self.assertEqual(step["expect"]["column"], 2)
                self.assertEqual(step["expect"]["formulaDraft"], "Income Statement:")
                self.assertEqual(step["evidence"]["sourceFrame"], source["sourceFrame"])
                self.assertEqual(hashlib.sha256((ROOT / source["sourceFrame"]).read_bytes()).hexdigest(), source["sourceFrameSha256"])

    def test_source_lower_cashflow_127_currency_uses_native_format_only(self):
        changes = [step for step in self.records if step["id"].endswith("demo-source-net-cash-currency")]
        self.assertEqual(len(changes), 1, "The lower 24.8-second view exposes missing currency glyphs on the published net-cash row")
        self.assertEqual(changes[0]["action"]["type"], "modal")
        self.assertEqual(changes[0]["evidence"]["kind"], "acceptance")
        self.assertEqual(changes[0]["videoTime"], "00:20.000")
        source_view = next(step for step in self.records if step["videoTime"] == "00:24.800" and step["id"].endswith("intro-inspection-viewport"))
        for column in "CDEFGHIJ":
            check = next(cell for cell in source_view["expect"]["cells"] if cell["address"] == column + "127")
            self.assertEqual(check["input"], "=" + "+".join(column + str(row) for row in [110, 115, 123, 125]))
            self.assertEqual(check["style"]["numberFormat"], DOLLAR_ACCOUNTING)
        self.assertTrue(all(not (step["action"]["type"] == "text" and step["action"].get("text") in ["0", "113", "342", "1676"]) for step in changes), "Formatting must not replace actual net-cash formulas or cache values")

    def test_all_twenty_source_plain_drivers_have_actual_style_checks(self):
        plain = {cell["address"] for cell in self.checks if cell.get("style", {}).get("bold") is False}
        self.assertTrue(set(PLAIN_DRIVERS) <= plain, "The published first-forecast bold style must not pass as the video's plain intro drivers")
        self.assertTrue(any(cell["address"] == "F2" and cell.get("style", {}).get("bold") is True for cell in self.checks), "The correction must preserve bold date headers")
        for address in ["G15", "J15", "G46", "J46"]:
            self.assertIn(address, plain, "Neighboring forecast input styles must stay plain")

    def test_source_blank_scratch_row_has_no_inputs_or_display(self):
        for address in ["F111", "G111", "H111"]:
            self.assertTrue(any(cell["address"] == address and cell.get("input") == "" and cell.get("display") == "" for cell in self.checks), "The video has no unlabeled scratch sums at " + address)

    def test_zero_balance_check_has_no_currency_and_keeps_formulas(self):
        for column in "DEFGHIJ":
            self.assertTrue(any(cell["address"] == column + "101" and cell.get("value") == 0 and cell.get("input") == "=" + column + "84-" + column + "99" and cell.get("style", {}).get("numberFormat") == NONE_ACCOUNTING for cell in self.checks), "Source zero dashes must retain the real formula without a currency glyph")
        self.assertTrue(any(cell["address"] == "C101" and cell.get("input") == "" for cell in self.checks), "The originally blank first-year balance check must stay blank")

    def test_source_net_income_to_parent_is_bold_across_all_years(self):
        for column in "CDEFGHIJ":
            self.assertTrue(any(cell["address"] == column + "105" and cell.get("input") == "=" + column + "73" and cell.get("style", {}).get("bold") is True for cell in self.checks), "Source Net Income to Parent is bold at " + column + "105")

    def test_three_source_zero_dashes_use_native_automatic_font_preparation(self):
        changes = [step for step in self.records if step["id"].endswith("demo-source-dark-zero")]
        self.assertEqual(len(changes), 3, "The source's three dark historical zero dashes must not inherit the published blue font")
        self.assertEqual({step["expect"]["cells"][0]["address"] for step in changes}, {"C119", "C120", "D120"})
        for step in changes:
            self.assertEqual(step["action"]["type"], "modal")
            self.assertEqual(step["action"]["open"], {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+1"]})
            self.assertEqual([child["action"] for child in step["action"]["steps"]], [{"type": "mouse_tab", "target": "formatCells.tabs", "tab": "Font"}, {"type": "key_sequence", "target": "formatCells.color", "keys": ["Ctrl+A"]}, {"type": "key_sequence", "target": "formatCells.color", "keys": ["Backspace"]}, {"type": "key_sequence", "target": "formatCells.dialog", "keys": ["Return"]}], "Automatic font reset must use real visible Font controls and preserve QAT color history")
            self.assertEqual(step["evidence"]["kind"], "acceptance")
            self.assertEqual(step["videoTime"], "00:20.000")
            self.assertEqual(step["expect"]["cells"][0]["style"], {"color": "", "numberFormat": NONE_ACCOUNTING})

    def test_dark_zero_dashes_are_checked_in_the_visible_cashflow_view(self):
        view = next(step for step in self.records if step["videoTime"] == "00:24.733" and step["id"].endswith("intro-inspection-viewport"))
        for address in ["C119", "C120", "D120"]:
            with self.subTest(address=address):
                self.assertTrue(any(cell["address"] == address and cell.get("input") == "0" and cell.get("value") == 0 and cell.get("style", {}).get("color") == "" for cell in view["expect"]["cells"]), "Source dark dash needs a real visible-view check at " + address)

    def test_source_intro_labels_are_checked_in_their_visible_views(self):
        for time, address, value in [("00:24.067", "B86", "LIABILITIES & EQUITY:"), ("00:24.733", "B108", "(+/-) Pensions & Other Items:")]:
            with self.subTest(address=address):
                view = next(step for step in self.records if step["videoTime"] == time and step["id"].endswith("intro-inspection-viewport"))
                self.assertIn({"address": address, "input": value}, view["expect"]["cells"], "The source intro's labels differ from the published template and must be verified on the actual visible sheet")

    def test_source_intro_labels_use_only_demo_native_edit_preparation(self):
        for address, value in [("B86", "LIABILITIES & EQUITY:"), ("B108", "(+/-) Pensions & Other Items:")]:
            with self.subTest(address=address):
                commits = [step for step in self.records if step["videoTime"] == "00:20.000" and {"address": address, "input": value} in step.get("expect", {}).get("cells", [])]
                self.assertEqual(len(commits), 1, "A source variant must be prepared by one real native editor commit, without cloning workbook metadata")
                commit = commits[0]
                self.assertEqual(commit["action"], {"type": "key_sequence", "target": "worksheet.cellEditor", "keys": ["Return"]})
                self.assertEqual(commit["evidence"]["kind"], "acceptance")

    def test_corrections_are_explicit_demo_only_native_preparation(self):
        corrected = [step for step in self.records if "-demo-source-" in step["id"]]
        self.assertTrue(corrected, "Source corrections need real native preparation actions, not only optimistic checks")
        self.assertTrue(all(step["videoTime"] == "00:20.000" and step["evidence"]["kind"] == "acceptance" for step in corrected))
        self.assertTrue(all(step["action"]["type"] in ["key_sequence", "modal"] for step in corrected))
        for name, reference in [("101-01-90-Minute-Case-Study-Otis.xlsx", "101-01-90-Minute-Case-Study-Otis-Complete.xlsx"), ("Otis-10-K-Raw.xls", "101-01-Otis-10-K-Raw.xls")]:
            sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(sha(self.work / name), sha(ROOT / "docs/validation/otis-90-minute/reference" / reference), "Generating the replay may copy untouched inputs but must not edit their cells/styles")
        self.assertFalse((self.work / "101-01-90-Minute-Case-Study-Otis-Blank.xlsx").exists(), "The generator must never construct the blank model")


if __name__ == "__main__":
    unittest.main()
