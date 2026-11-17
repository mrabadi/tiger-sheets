import importlib.util
import copy
import json
import subprocess
import sys
import struct
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from xml.etree import ElementTree as ET
from zipfile import ZipFile
import openpyxl

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("audit_otis_native_final", ROOT / "tools/audit_otis_native_final.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)
NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}


def fixture(path, cache="3", number_format=None):
    book = openpyxl.Workbook()
    sheet = book.active
    sheet.title = "Sheet1"
    sheet["A1"] = 2
    sheet["B1"] = "=A1+1"
    if number_format is not None:
        sheet["B1"].number_format = number_format
    book.save(path)
    with ZipFile(path) as source:
        parts = {name: source.read(name) for name in source.namelist()}
    root = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
    cell = root.find("m:sheetData/m:row/m:c[@r='B1']", NS)
    value = cell.find("m:v", NS)
    value.text = cache
    parts["xl/worksheets/sheet1.xml"] = ET.tostring(root)
    with ZipFile(path, "w") as target:
        for name, contents in parts.items():
            target.writestr(name, contents)


def add_external_link(path, target):
    with ZipFile(path) as source:
        parts = {name: source.read(name) for name in source.namelist()}
    workbook = ET.fromstring(parts["xl/workbook.xml"])
    refs = ET.SubElement(workbook, "{" + NS["m"] + "}externalReferences")
    ET.SubElement(refs, "{" + NS["m"] + "}externalReference", {"{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id": "rIdExternal"})
    parts["xl/workbook.xml"] = ET.tostring(workbook)
    rels = ET.fromstring(parts["xl/_rels/workbook.xml.rels"])
    ET.SubElement(rels, "{http://schemas.openxmlformats.org/package/2006/relationships}Relationship", {"Id": "rIdExternal", "Type": "http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLink", "Target": "externalLinks/externalLink1.xml"})
    parts["xl/_rels/workbook.xml.rels"] = ET.tostring(rels)
    parts["xl/externalLinks/externalLink1.xml"] = b'<externalLink xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalBook r:id="rId1"><sheetNames><sheetName val="operations"/></sheetNames></externalBook></externalLink>'
    parts["xl/externalLinks/_rels/externalLink1.xml.rels"] = ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLinkPath" TargetMode="External" Target="' + target + '"/></Relationships>').encode()
    sheet = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
    sheet.find("m:sheetData/m:row/m:c[@r='B1']/m:f", NS).text = "'[1]operations'!G8"
    parts["xl/worksheets/sheet1.xml"] = ET.tostring(sheet)
    with ZipFile(path, "w") as archive:
        for name, contents in parts.items(): archive.writestr(name, contents)


def shared_formula_fixture(path, shared=True):
    book = openpyxl.Workbook()
    sheet = book.active
    sheet.title = "Sheet1"
    book.create_sheet("Data Space")
    formulas = {
        "B1": '=SUM(A1,$A1,A$1,$A$1,\'Data Space\'!A1,"A1")',
        "B2": '=SUM(A2,$A2,A$1,$A$1,\'Data Space\'!A2,"A1")',
        "C1": '=SUM(B1,$A1,B$1,$A$1,\'Data Space\'!B1,"A1")',
        "C2": '=SUM(B2,$A2,B$1,$A$1,\'Data Space\'!B2,"A1")',
    }
    for address, formula in formulas.items():
        sheet[address] = formula
    book.save(path)
    with ZipFile(path) as archive:
        parts = {name: archive.read(name) for name in archive.namelist()}
    root = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
    for cell in root.findall("m:sheetData/m:row/m:c", NS):
        cell.find("m:v", NS).text = "3"
        if shared:
            formula = cell.find("m:f", NS)
            formula.attrib.update({"t": "shared", "si": "0"})
            if cell.get("r") == "B1":
                formula.set("ref", "B1:C2")
            else:
                formula.text = None
    parts["xl/worksheets/sheet1.xml"] = ET.tostring(root)
    with ZipFile(path, "w") as archive:
        for name, contents in parts.items(): archive.writestr(name, contents)
    return formulas


def history_fixture(path, formula="='[1]operations'!G8"):
    fixture(path, cache="5648")
    add_external_link(path, "Otis-10-K-Raw.xls")
    with ZipFile(path) as source:
        parts = {name: source.read(name) for name in source.namelist()}
    root = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
    row = root.find("m:sheetData/m:row[@r='1']", NS)
    cell = row.find("m:c[@r='B1']", NS)
    row.remove(cell)
    cell.set("r", "C54")
    cell.find("m:f", NS).text = formula[1:]
    ET.SubElement(root.find("m:sheetData", NS), "{" + NS["m"] + "}row", {"r": "54"}).append(cell)
    parts["xl/worksheets/sheet1.xml"] = ET.tostring(root)
    with ZipFile(path, "w") as target:
        for name, contents in parts.items(): target.writestr(name, contents)


class NativeFinalAuditTests(unittest.TestCase):
    def test_biff_saved_view_inventory_reads_native_window_selection_and_zoom_records(self):
        try:
            import xlrd
        except ImportError:
            self.skipTest("Run source audit tests with PYTHONPATH containing xlrd")
        raw = ROOT / "docs/validation/otis-90-minute/reference/101-01-Otis-10-K-Raw.xls"
        inventory = audit.read_xls(raw)
        self.assertEqual(inventory.get("activeSheetIndex"), 0, "WINDOW1 active tab must be independently read rather than assumed")
        self.assertEqual(inventory["sheets"][36].get("nativeView", {}).get("topRow"), 1)
        record = lambda kind, data: struct.pack("<HH", kind, len(data)) + data
        window1 = record(0x003D, struct.pack("<9H", 0, 0, 16384, 8192, 56, 0, 0, 1, 600))
        bound = record(0x0085, struct.pack("<I4B", len(window1) + 12 + 4, 0, 0, 0, 0))
        window2 = record(0x023E, struct.pack("<9H", 0x6B6, 36, 0, 64, 0, 0, 100, 0, 0))
        selection = record(0x001D, struct.pack("<B4H2H2B", 3, 50, 4, 0, 1, 50, 50, 4, 4))
        stream = window1 + bound + record(0x000A, b"") + window2 + selection + record(0x00A0, struct.pack("<2H", 100, 100)) + record(0x000A, b"")
        view = audit.biff_views_from_stream(stream)["sheets"][0]
        self.assertEqual(view["activeCell"], "E51")
        self.assertEqual(view["selectedRanges"], ["E51"])
        self.assertEqual((view["topRow"], view["firstColumn"], view["zoomPercent"]), (37, "A", 100))
        self.assertTrue(view["showGridLines"] and view["showHeadings"] and view["selected"] and view["displayed"])

    def test_primary_source_mapped_styles_and_unmerged_state_are_required(self):
        reference = {"sheetCount": 1, "sha256": "original", "styleDefinitions": {}, "sheets": [{"name": "operations", "merges": [[7, 8, 6, 9]], "cells": {"G8": {"row": 8, "column": 7, "type": 2, "value": 6428, "style": {"font": "Calibri11", "fill": "none"}}}}]}
        actual = copy.deepcopy(reference)
        cell = actual["sheets"][0]["cells"].pop("G8")
        cell["column"] = 9
        actual["sheets"][0]["cells"]["I8"] = cell
        cell["style"]["fill"] = "red"
        required = {"preserveProjectedPrimaryStyles": True, "primaryMerges": {"operations": []}}
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            result = audit.audit_source_xls("actual.xls", "raw.xls", source_oracle=required)
        self.assertFalse(result["passed"])
        self.assertEqual({item["code"] for item in result["failures"]}, {"edited_source_style", "edited_source_merges"})
        cell["style"]["fill"] = "none"
        actual["sheets"][0]["merges"] = []
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            result = audit.audit_source_xls("actual.xls", "raw.xls", source_oracle=required)
        self.assertTrue(result["passed"], result["failures"])
        self.assertEqual(result["matchedPrimaryStyles"], 1)

    def test_final_source_view_is_independent_of_synthetic_view_test_artifacts(self):
        expected = {"sheetIndex": 36, "sheetName": "cash flows", "activeCell": "E51", "selectedRanges": ["E51"], "topRow": 37, "firstColumn": "A", "zoomPercent": 100, "showGridLines": True, "showHeadings": True}
        sheets = [{"name": "untouched" + str(index), "cells": {}, "merges": [], "nativeView": {"selected": False, "displayed": False}, "defaultStyleIndex": "15", "rowStyleIndices": {}, "columnStyleIndices": {}} for index in range(37)]
        sheets[36].update(name="cash flows", nativeView={key: value for key, value in expected.items() if key not in ("sheetIndex", "sheetName")})
        sheets[36]["nativeView"].update(selected=True, displayed=True)
        original = {"sheetCount": 37, "sha256": "original", "styleDefinitions": {"15": {}}, "sheets": sheets, "activeSheetIndex": 36}
        for field, wrong in [("activeCell", "A1"), ("selectedRanges", ["E51:G53"]), ("topRow", 1), ("firstColumn", "B"), ("zoomPercent", 125), ("showGridLines", False), ("showHeadings", False), ("selected", False), ("displayed", False), ("activeSheetIndex", 0)]:
            with self.subTest(field=field):
                actual = copy.deepcopy(original)
                if field == "activeSheetIndex": actual[field] = wrong
                else: actual["sheets"][36]["nativeView"][field] = wrong
                with patch.object(audit, "read_xls", side_effect=[actual, original]):
                    result = audit.audit_source_xls("actual.xls", "raw.xls", source_oracle={"view": expected})
                self.assertFalse(result["passed"], "The final original source frame fixes cash flows/E51/A37/100%, not the intentionally synthetic B37/125%/E51:G53 test state")
        with patch.object(audit, "read_xls", side_effect=[original, original]):
            result = audit.audit_source_xls("actual.xls", "raw.xls", source_oracle={"view": expected})
        self.assertTrue(result["passed"], result["failures"])

    def test_source_primary_layout_requires_typed_spacer_and_visible_original_rows(self):
        before_layout = {"defaultRowHeightTwips": 300, "rowHeights": {}, "hiddenRows": [], "hiddenColumns": [], "columnWidths": {"G": 2224, "M": 2224, "S": 2224}}
        after_layout = {**copy.deepcopy(before_layout), "columnWidths": {"B": 694, "C": 2224, "D": 2224, "E": 2224}}
        reference = {"sheetCount": 1, "sha256": "raw", "styleDefinitions": {}, "sheets": [{"name": "cash flows", "cells": {}, "merges": [], "layout": before_layout}]}
        actual = copy.deepcopy(reference)
        actual["sheets"][0]["layout"] = after_layout
        expected = {"primaryLayout": {"cash flows": {"defaultRowHeightTwips": 300, "rowHeightTwips": 300, "hiddenRows": [], "hiddenColumns": [], "columnWidths": {"B": 694}, "preservedColumnWidths": {"C": "S", "D": "M", "E": "G"}}}}
        changes = [("defaultRowHeightTwips", 315), ("rowHeights", {"51": 400}), ("hiddenRows", [51]), ("hiddenColumns", [3]), ("columnWidths", {"B": 512, "C": 2224, "D": 2224, "E": 2224}), ("columnWidths", {"B": 694, "C": 3000, "D": 2224, "E": 2224})]
        for field, value in changes:
            with self.subTest(field=field, value=value):
                changed = copy.deepcopy(actual)
                changed["sheets"][0]["layout"][field] = value
                with patch.object(audit, "read_xls", side_effect=[changed, reference]):
                    self.assertFalse(audit.audit_source_xls("actual.xls", "raw.xls", source_oracle=expected)["passed"], "Typed source width, original row heights and visible dimensions cannot be waived by matching values")
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            result = audit.audit_source_xls("actual.xls", "raw.xls", source_oracle=expected)
        self.assertTrue(result["passed"], result["failures"])

    def test_exact_historical_source_formula_cannot_be_replaced_by_same_value_expression(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            oracle = {"independentExactHistoricalValues": [{"address": "C54", "value": 5648}]}
            history_fixture(path)
            self.assertTrue(audit.audit_xlsx(path, oracle=oracle)["passed"])
            for formula in ["='[1]operations'!G8+0", "=5648+0*'[1]operations'!G8", "='[1]operations'!H8"]:
                with self.subTest(formula=formula):
                    history_fixture(path, formula)
                    result = audit.audit_xlsx(path, oracle=oracle)
                    self.assertFalse(result["passed"], "A correct cache cannot waive an incorrect source-observed historical formula")
                    self.assertTrue(any(failure["code"] == "source_input" and failure["address"] == "C54" for failure in result["failures"]))

    def test_all134_historical_formulas_have_independent_source_patterns(self):
        oracle = json.loads((ROOT / "docs/validation/otis-90-minute/final-native-acceptance-oracle.json").read_text())
        formulas = {item["address"]: item["input"] for item in audit.historical_formula_checks(oracle)}
        self.assertEqual(set(formulas), {item["address"] for item in oracle["independentExactHistoricalValues"]})
        self.assertEqual(len(formulas), 134)
        self.assertEqual(formulas["C54"], "='[Otis-10-K-Raw.xls]operations'!G8")
        self.assertEqual(formulas["E65"], "=-'[Otis-10-K-Raw.xls]operations'!I20")
        self.assertEqual(formulas["D78"], "='[Otis-10-K-Raw.xls]consolidated balance sheets'!B9")
        self.assertEqual(formulas["E91"], "='[Otis-10-K-Raw.xls]consolidated balance sheets'!C29+'[Otis-10-K-Raw.xls]consolidated balance sheets'!C31+'[Otis-10-K-Raw.xls]consolidated balance sheets'!C32")
        self.assertEqual(formulas["C117"], "=SUM('[Otis-10-K-Raw.xls]cash flows'!C36:C39)+'[Otis-10-K-Raw.xls]cash flows'!C41")
        self.assertEqual(formulas["E126"], "=E109+E114+E122+E124")

    def test_saved_B2_active_cell_requires_exact_single_cell_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            book = openpyxl.Workbook()
            book.active.title = "Sheet1"
            book.active["B2"] = "Drivers:"
            view = book.active.sheet_view
            view.selection[0].activeCell = "B2"
            oracle = {"view": {"sheetName": "Sheet1", "activeCell": "B2", "topRow": 1, "firstColumn": "A"}}
            for sqref in ["B2:C3", "A1:XFD1048576", "A1 B2"]:
                with self.subTest(sqref=sqref):
                    view.selection[0].sqref = sqref
                    book.save(path)
                    result = audit.audit_xlsx(path, oracle=oracle)
                    self.assertFalse(result["passed"], "B2 as current cell cannot waive a larger saved selection")
            view.selection[0].sqref = "B2"
            book.save(path)
            self.assertTrue(audit.audit_xlsx(path, oracle=oracle)["passed"])

    def test_stored_cells_without_explicit_style_use_native_dimension_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            book = openpyxl.Workbook()
            sheet = book.active
            sheet["A1"] = 7
            sheet["A2"] = 8
            sheet.column_dimensions["A"].font = openpyxl.styles.Font(name="Calibri", size=12)
            sheet.column_dimensions["A"].fill = openpyxl.styles.PatternFill("solid", fgColor="FF0000")
            book.save(path)
            with ZipFile(path) as source: parts = {name: source.read(name) for name in source.namelist()}
            root = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
            root.find("m:sheetData/m:row/m:c[@r='A2']", NS).set("s", "0")
            parts["xl/worksheets/sheet1.xml"] = ET.tostring(root)
            with ZipFile(path, "w") as target:
                for name, contents in parts.items(): target.writestr(name, contents)
            inventory = audit.read_xlsx(path)
            inherited = inventory["cells"]["Sheet!A1"]["style"]
            explicit_normal = inventory["cells"]["Sheet!A2"]["style"]
            self.assertEqual(inherited["font"]["size"], 12)
            self.assertEqual(inherited["fill"], {"pattern": "solid", "color": "#FF0000"})
            self.assertEqual(explicit_normal["font"]["size"], 11)
            self.assertIsNone(explicit_normal["fill"]["pattern"], "An explicit native style0 overrides the column XF")
            sheet.row_dimensions[1].font = openpyxl.styles.Font(name="Calibri", size=14)
            book.save(path)
            self.assertEqual(audit.read_xlsx(path)["cells"]["Sheet!A1"]["style"]["font"]["size"], 14, "Native row XF takes priority over the default column XF")

    def test_unstored_row_and_column_formatting_is_compared_beyond_stored_union(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            for kind in ["column_fill", "row_number_format"]:
                with self.subTest(kind=kind):
                    book = openpyxl.Workbook()
                    sheet = book.active
                    sheet["A1"] = "Keep"
                    book.save(reference)
                    if kind == "column_fill":
                        sheet.column_dimensions["K"].fill = openpyxl.styles.PatternFill("solid", fgColor="FF0000")
                    else:
                        sheet.row_dimensions[1000].number_format = "0.00"
                    book.save(actual)
                    result = audit.audit_xlsx(actual, reference=reference)
                    self.assertFalse(result["passed"], "A changed default fill/number format on entirely unstored cells is still a source-visible mismatch")
                    self.assertTrue(any(item["category"] == "unstored_effective_style" for item in result["publishedComparison"]["unexplainedDifferences"]))

    def test_missing_styled_biff_blank_is_compared_to_effective_native_default(self):
        normal = {"fill": "none"}
        gray = {"fill": "gray"}
        reference = {"sheetCount": 1, "sha256": "untouched-source", "styleDefinitions": {"15": normal}, "sheets": [{"name": "untouched", "merges": [], "cells": {"B2": {"row": 2, "column": 2, "type": 6, "value": "", "style": gray}}, "defaultStyleIndex": "15", "rowStyleIndices": {}, "columnStyleIndices": {}}]}
        actual = copy.deepcopy(reference)
        actual["sheets"][0]["cells"].clear()
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            result = audit.audit_source_xls("actual.xls", "reference.xls")
        self.assertFalse(result["passed"], "Removing an untouched gray BIFF blank must not waive its visible fill")
        self.assertTrue(any(item["code"] == "unmodified_source_style" for item in result["failures"]))
        actual["styleDefinitions"]["16"] = gray
        actual["sheets"][0]["columnStyleIndices"]["2"] = "16"
        reference["styleDefinitions"]["16"] = gray
        reference["sheets"][0]["columnStyleIndices"]["2"] = "16"
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            self.assertTrue(audit.audit_source_xls("actual.xls", "reference.xls")["passed"], "An omitted blank is equivalent only when its actual inherited BIFF style matches")

    def test_untouched_biff_unstored_column_default_cannot_silently_change(self):
        reference = {"sheetCount": 1, "sha256": "untouched-source", "styleDefinitions": {"15": {"fill": "none"}, "16": {"fill": "red"}}, "sheets": [{"name": "untouched", "merges": [], "cells": {}, "defaultStyleIndex": "15", "rowStyleIndices": {}, "columnStyleIndices": {}}]}
        actual = copy.deepcopy(reference)
        actual["sheets"][0]["columnStyleIndices"]["11"] = "16"
        with patch.object(audit, "read_xls", side_effect=[actual, reference]):
            result = audit.audit_source_xls("actual.xls", "reference.xls")
        self.assertFalse(result["passed"], "A red inherited fill on an otherwise empty untouched BIFF column is still a source-file regression")
        self.assertTrue(any(item["code"] == "unmodified_source_blank_style" for item in result["failures"]))

    def test_source_whole_sheet_font_covers_unstored_columns(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            book = openpyxl.Workbook()
            sheet = book.active
            sheet["A1"] = "Keep"
            sheet["A1"].font = openpyxl.styles.Font(name="Calibri", size=12)
            sheet.column_dimensions["A"].font = openpyxl.styles.Font(name="Calibri", size=12)
            book.save(path)
            oracle = {"sheetDefaults": {"fontFamily": "Calibri", "fontSize": 12}}
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "Formatting only the stored first column must not pass a whole-worksheet font oracle")
            sheet.column_dimensions["A"].min = 1
            sheet.column_dimensions["A"].max = 16384
            book.save(path)
            result = audit.audit_xlsx(path, oracle=oracle)
            self.assertTrue(result["passed"], result["failures"])

    def test_source_default_row_height_is_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            book = openpyxl.Workbook()
            book.active["A1"] = "Keep"
            book.save(path)
            oracle = {"sheetDefaults": {"defaultRowHeight": 15.75}}
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "The source21px rows require the actual15.75pt native default")
            book.active.sheet_format.defaultRowHeight = 15.75
            book.save(path)
            result = audit.audit_xlsx(path, oracle=oracle)
            self.assertTrue(result["passed"], result["failures"])

    def test_missing_blank_cell_does_not_waive_visible_fill_and_border(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            book = openpyxl.Workbook()
            book.active["A1"] = "Keep"
            book.save(actual)
            book.active["B2"].fill = openpyxl.styles.PatternFill("solid", fgColor="D9D9D9")
            book.active["B2"].border = openpyxl.styles.Border(top=openpyxl.styles.Side(style="thin", color="000000"))
            book.save(reference)
            before = actual.read_bytes(), reference.read_bytes()
            result = audit.audit_xlsx(actual, reference=reference)
            differences = result["publishedComparison"]["unexplainedDifferences"]
            self.assertFalse(result["passed"], "A missing gray bordered blank cell is a visible formatting failure")
            self.assertTrue(any(item["address"] == "Sheet!B2" and item["category"] == "effective_style" for item in differences))
            self.assertEqual((actual.read_bytes(), reference.read_bytes()), before)

    def test_blank_storage_uses_native_row_and_column_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            book = openpyxl.Workbook()
            sheet = book.active
            sheet["A1"] = "Keep"
            sheet.column_dimensions["B"].font = openpyxl.styles.Font(name="Calibri", size=12)
            sheet.row_dimensions[2].font = openpyxl.styles.Font(name="Calibri", size=14)
            book.save(actual)
            sheet["B2"].font = openpyxl.styles.Font(name="Calibri", size=14)
            book.save(reference)
            result = audit.audit_xlsx(actual, reference=reference)
            self.assertTrue(result["passed"], result["failures"])

    def test_shared_formula_inventory_expands_rows_columns_anchors_and_quoted_text(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "shared.xlsx"
            expected = shared_formula_fixture(path)
            before = path.read_bytes()
            inventory = audit.read_xlsx(path)
            for address, formula in expected.items():
                cell = inventory["cells"]["Sheet1!" + address]
                self.assertEqual(cell["formula"], formula)
                self.assertEqual(cell["input"], formula)
                self.assertEqual(cell["cachedValue"], 3)
            self.assertEqual(inventory["formulaCells"], 4)
            self.assertEqual(path.read_bytes(), before)

    def test_shared_and_explicit_formula_files_compare_as_same_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "explicit.xlsx", Path(directory) / "shared.xlsx"
            shared_formula_fixture(actual, shared=False)
            shared_formula_fixture(reference)
            result = audit.audit_xlsx(actual, reference=reference)
            self.assertTrue(result["passed"], result["failures"])

    def test_inventory_reads_every_stored_cell_and_formula_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            before = path.read_bytes()
            result = audit.audit_xlsx(path)
            self.assertEqual(result["inventory"]["storedCells"], 2)
            self.assertEqual(result["inventory"]["formulaCells"], 1)
            self.assertEqual(result["inventory"]["formulaCaches"], 1)
            self.assertEqual(result["inventory"]["cells"]["Sheet1!B1"]["formula"], "=A1+1")
            self.assertEqual(result["inventory"]["cells"]["Sheet1!B1"]["cachedValue"], 3)
            self.assertEqual(path.read_bytes(), before)


    def test_source_value_cache_and_type_corruption_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            oracle = {"directNativeCheckpoint": {"expect": {"cells": [{"address": "B1", "value": 3}]}}}
            for value in ("4", None):
                with self.subTest(cache=value):
                    fixture(path, cache=value)
                    self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "Wrong or absent native formula caches must fail the source oracle")

    def test_rounded_source_display_is_not_exact_cache_equality(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            oracle = {"sourceRoundedDisplays": [{"address": "B1", "displayedNumber": 3, "decimalPlaces": 0}]}
            fixture(path, cache="3.2", number_format="0")
            self.assertTrue(audit.audit_xlsx(path, oracle=oracle)["passed"])
            fixture(path, cache="3.6", number_format="0")
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "A source-visible rounded digit mismatch must fail")


    def test_published_cache_difference_is_reported_and_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            fixture(actual, cache="4")
            fixture(reference, cache="3")
            result = audit.audit_xlsx(actual, reference=reference)
            self.assertFalse(result["passed"], "Every stored formula cache must be independently compared")
            self.assertTrue(result["publishedComparison"]["unexplainedDifferences"])

    def test_source_oracle_is_not_mutated_by_repeat_audits(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            oracle = {"directNativeCheckpoint": {"expect": {"cells": [{"address": "B1", "value": 3}]}}, "independentExactHistoricalValues": [{"address": "A1", "value": 2}]}
            before = copy.deepcopy(oracle)
            audit.audit_xlsx(path, oracle=oracle)
            audit.audit_xlsx(path, oracle=oracle)
            self.assertEqual(oracle, before, "A read-only audit also preserves its source evidence object")


    def test_source_saved_view_and_style_mismatch_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            view = {"view": {"sheetName": "Sheet1", "activeCell": "B1", "topRow": 1, "firstColumn": "A", "showGridLines": False, "zoomPercent": 100}}
            self.assertFalse(audit.audit_xlsx(path, oracle=view)["passed"], "A different saved selection or gridline view must fail")

    def test_source_style_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            style = {"styleRanges": [{"ranges": ["A1:B1"], "style": {"fontSize": 12}}]}
            self.assertFalse(audit.audit_xlsx(path, oracle=style)["passed"], "Source-visible formatting must be verified against effective native styles")

    def test_indexed_external_formulas_resolve_to_relative_actual_filename(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            add_external_link(path, "Otis-10-K-Raw.xls")
            oracle = {"exactFormulaChecks": [{"address": "B1", "input": "='[Otis-10-K-Raw.xls]operations'!G8"}]}
            self.assertTrue(audit.audit_xlsx(path, oracle=oracle)["passed"], "Indexed native external formulas resolve to actual source filenames")
            fixture(path)
            add_external_link(path, "/tmp/Otis-10-K-Raw.xls")
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "A nonportable absolute external target must fail")


    def test_uncleaned_raw_source_cannot_pass_as_final_retained_source(self):
        try:
            import xlrd
        except ImportError:
            self.skipTest("Run source audit tests with PYTHONPATH containing xlrd")
        raw = ROOT / "docs/validation/otis-90-minute/reference/101-01-Otis-10-K-Raw.xls"
        result = audit.audit_source_xls(raw, raw)
        self.assertFalse(result["passed"], "The retained source must contain observed year-column cleaning and emdash corrections")

    def test_actual_native_cleaned_source_preserves_all120_sheets(self):
        try:
            import xlrd
        except ImportError:
            self.skipTest("Run source audit tests with PYTHONPATH containing xlrd")
        raw = ROOT / "docs/validation/otis-90-minute/reference/101-01-Otis-10-K-Raw.xls"
        clean = ROOT / "docs/validation/otis-90-minute/native-history/checkpoint-35-11/Otis-10-K-Raw.xls"
        if not clean.exists():
            self.skipTest("Native replay checkpoint is not available")
        result = audit.audit_source_xls(clean, raw)
        self.assertTrue(result["passed"], result.get("failures"))
        self.assertEqual(result["actualSheetCount"], 120)


    def test_full_audit_cli_writes_machine_report_and_rejects_bad_cache(self):
        try:
            import xlrd
        except ImportError:
            self.skipTest("Run source audit tests with PYTHONPATH containing xlrd")
        raw = ROOT / "docs/validation/otis-90-minute/reference/101-01-Otis-10-K-Raw.xls"
        clean = ROOT / "docs/validation/otis-90-minute/native-history/checkpoint-35-11/Otis-10-K-Raw.xls"
        if not clean.exists():
            self.skipTest("Native replay checkpoint is not available")
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            actual, reference, oracle, output = [folder / name for name in ["actual.xlsx", "reference.xlsx", "oracle.json", "audit.json"]]
            fixture(actual)
            fixture(reference)
            oracle.write_text("{}")
            command = [sys.executable, str(ROOT / "tools/audit_otis_native_final.py"), str(actual), "--source", str(clean), "--reference", str(reference), "--raw-reference", str(raw), "--oracle", str(oracle), "--output", str(output)]
            before = actual.read_bytes()
            first = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertTrue(output.exists(), "The runnable independent audit writes its complete machine-readable evidence report")
            self.assertTrue(json.loads(output.read_text())["passed"])
            self.assertEqual(actual.read_bytes(), before)
            fixture(actual, cache="4")
            second = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(second.returncode, 1, "A concrete value corruption must produce a nonzero audit status")

    def test_required_external_source_link_cannot_be_missing(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            self.assertFalse(audit.audit_xlsx(path, oracle={"requireExternalSourceLink": True})["passed"])


    def test_source_observed_font_color_overrides_published_input_color_only(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            fixture(actual)
            fixture(reference)
            book = openpyxl.load_workbook(reference)
            book.active["A1"].font = openpyxl.styles.Font(name="Calibri", size=11, color="0000FF")
            book.save(reference)
            # openpyxl cannot calculate caches; restore the untouched B1cache.
            with ZipFile(reference) as archive: parts = {name: archive.read(name) for name in archive.namelist()}
            root = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
            root.find("m:sheetData/m:row/m:c[@r='B1']/m:v", NS).text = "3"
            parts["xl/worksheets/sheet1.xml"] = ET.tostring(root)
            with ZipFile(reference, "w") as archive:
                for name, contents in parts.items(): archive.writestr(name, contents)
            oracle = {"styleRanges": [{"ranges": ["A1:A1"], "color": "#000000"}]}
            result = audit.audit_xlsx(actual, reference=reference, oracle=oracle)
            self.assertTrue(result["passed"], "Observed source black text takes precedence over published blue inputs")
            self.assertTrue(any(item["category"] == "source_style_override" for item in result["publishedComparison"]["explainedDifferences"]))


    def test_source_display_format_cannot_be_assumed_from_numeric_rounding(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            for cache, fields in [("3", {"displayedNumber": 3, "decimalPlaces": 2}), ("3", {"displayedNumber": 3, "decimalPlaces": 0, "currencySymbol": True}), ("-3", {"displayedNumber": -3, "decimalPlaces": 0, "negativePresentation": "parentheses"}), ("0.03", {"displayedNumber": 3, "decimalPlaces": 0, "unit": "percent"})]:
                with self.subTest(fields=fields):
                    fixture(path, cache=cache)
                    oracle = {"sourceRoundedDisplays": [{"address": "B1", **fields}]}
                    self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "A matching rounded number with wrong visible format must fail")


    def test_exact_balance_zero_cannot_pass_as_a_tiny_residual(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path, cache="0.0000000000001")
            oracle = {"calculationChecks": [{"range": "B1:B1", "value": 0, "exactZero": True}]}
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "The final native balance zero must remain exactly zero")

    def test_source_negative_rounded_zero_retains_actual_residual_sign(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            oracle = {"sourceRoundedDisplays": [{"address": "B1", "displayedNumber": 0, "decimalPlaces": 0, "roundedZeroSign": "negative", "negativePresentation": "parentheses"}]}
            fixture(path, cache="0", number_format="0;(0);\"-\"")
            self.assertFalse(audit.audit_xlsx(path, oracle=oracle)["passed"], "The source's (0) cannot be replaced by exact zero, which displays an Accounting dash")
            fixture(path, cache="-0.000000000001", number_format="0;(0);\"-\"")
            self.assertTrue(audit.audit_xlsx(path, oracle=oracle)["passed"])

    def test_exact_numeric_cache_differences_are_retained_inside_tolerance(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            fixture(actual, cache="3.0000000000001")
            fixture(reference, cache="3")
            result = audit.audit_xlsx(actual, reference=reference)
            self.assertTrue(result["passed"])
            differences = result["publishedComparison"].get("exactNumericCacheDifferences", [])
            self.assertEqual(len(differences), 1, "Every exact numeric cache difference is inventoried even when within accepted arithmetic tolerance")
            self.assertEqual(differences[0]["address"], "Sheet1!B1")

    def test_observed_fy_literal_space_remains_a_raw_code_difference(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            fixture(actual, number_format='"FY" yy')
            fixture(reference, number_format='"FY"\\ yy')
            oracle = {"styleRanges": [{"ranges": ["B1:B1"], "numberFormat": '"FY" yy'}]}
            result = audit.audit_xlsx(actual, reference=reference, oracle=oracle)
            self.assertTrue(result["passed"])
            differences = result["publishedComparison"].get("exactNumberFormatCodeDifferences", [])
            self.assertEqual(differences, [{"address": "Sheet1!B1", "published": '"FY"\\ yy', "actual": '"FY" yy', "classification": "source_observed_raw_code", "semanticEquivalent": True}])
            self.assertTrue(any(item["category"] == "source_style_override" for item in result["publishedComparison"]["explainedDifferences"]), "Equivalent FY displays must not be counted as identical raw style codes")

    def test_source_custom_percent_code_overrides_only_the_exact_published_code_pair(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            source_code, published_code = "0.0%;(0.0%)", r"0.0%;\(0.0%\)"
            fixture(actual, number_format=source_code)
            fixture(reference, number_format=published_code)
            result = audit.audit_xlsx(actual, reference=reference)
            self.assertTrue(result["passed"], "The exact Custom Type code retained at income079 takes precedence over the published escaped parentheses")
            self.assertEqual(result["publishedComparison"]["exactNumberFormatCodeDifferences"], [{
                "address": "Sheet1!B1", "published": published_code, "actual": source_code,
                "classification": "source_observed_raw_code", "semanticEquivalent": True,
            }])
            self.assertTrue(any(item["category"] == "source_style_override" for item in result["publishedComparison"]["explainedDifferences"]))

            actual_inventory, reference_inventory = audit.read_xlsx(actual), audit.read_xlsx(reference)
            actual_inventory["cells"]["Sheet1!B1"]["style"]["font"]["bold"] = True
            comparison = audit.compare_published(actual_inventory, reference_inventory, {})
            self.assertTrue(any(item["category"] == "effective_style" and "font.bold" in item["fields"] for item in comparison["unexplainedDifferences"]), "The known raw-code pair must not hide a separate font mismatch")

            fixture(actual, cache="4", number_format=source_code)
            self.assertTrue(any(item.get("category") == "formula_cache_or_literal_value" for item in audit.audit_xlsx(actual, reference=reference)["failures"]), "The format authority must not waive a different formula result")
            for other_actual, other_published in [("0.00%;(0.00%)", r"0.00%;\(0.00%\)"), (published_code, source_code)]:
                with self.subTest(actual=other_actual, published=other_published):
                    fixture(actual, number_format=other_actual)
                    fixture(reference, number_format=other_published)
                    self.assertFalse(audit.audit_xlsx(actual, reference=reference)["passed"], "No general escape normalization or reversed source authority is allowed")

    def test_source_literal_spacing_override_also_governs_its_literal_value_only(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            source_text = "Other Liabilities % Total Expenses:"
            published_text = "Other Liabilities %  Total Expenses:"

            def label(path, text):
                book = openpyxl.Workbook()
                book.active.title = "Sheet1"
                book.active["B29"] = text
                book.save(path)

            label(actual, source_text)
            label(reference, published_text)
            oracle = {"directNativeCheckpoint": {"expect": {"cells": [{"address": "B29", "input": source_text}]}}}
            result = audit.audit_xlsx(actual, reference=reference, oracle=oracle)
            self.assertTrue(result["passed"], "The exact one-space source input must also be the authority for that nonformula text value")
            self.assertTrue(any(item["category"] == "source_input_override" for item in result["publishedComparison"]["explainedDifferences"]))
            self.assertFalse(audit.audit_xlsx(actual, reference=reference)["passed"], "Unobserved literal differences still fail")
            for wrong in [published_text, " " + source_text, source_text + " ", source_text.replace(" % ", " %  ")]:
                with self.subTest(literal=wrong):
                    label(actual, wrong)
                    self.assertTrue(any(item["code"] == "source_input" for item in audit.audit_xlsx(actual, reference=reference, oracle=oracle)["failures"]), "Source text is exact, with no whitespace normalization")

            label(actual, source_text)
            actual_inventory, reference_inventory = audit.read_xlsx(actual), audit.read_xlsx(reference)
            actual_inventory["cells"]["Sheet1!B29"]["cachedValue"] = published_text
            self.assertTrue(any(item["category"] == "formula_cache_or_literal_value" for item in audit.compare_published(actual_inventory, reference_inventory, oracle)["unexplainedDifferences"]), "An accepted input cannot hide a different literal value")

            fixture(actual, cache="4")
            fixture(reference, cache="3")
            formula_oracle = {"directNativeCheckpoint": {"expect": {"cells": [{"address": "B1", "input": "=A1+1"}]}}}
            self.assertTrue(any(item.get("category") == "formula_cache_or_literal_value" for item in audit.audit_xlsx(actual, reference=reference, oracle=formula_oracle)["failures"]), "An exact source formula must not waive its incorrect result cache")
            text_formula = audit.read_xlsx(actual)
            text_formula["cells"]["Sheet1!B1"].update(formula=None, input="=A1+1", cachedValue="=A1+1")
            self.assertTrue(any(item["category"] == "formula_cache_or_literal_value" for item in audit.compare_published(text_formula, audit.read_xlsx(reference), formula_oracle)["unexplainedDifferences"]), "A formula-looking text literal cannot inherit authority intended for a real source formula")

    def test_unobserved_format_escapes_are_not_globally_normalized(self):
        with tempfile.TemporaryDirectory() as directory:
            actual, reference = Path(directory) / "actual.xlsx", Path(directory) / "reference.xlsx"
            for actual_code, published_code in [('"FY" yy', '"FY"\\ yy'), ('0;(0)', '0;\\(0\\)')]:
                with self.subTest(actual=actual_code):
                    fixture(actual, number_format=actual_code)
                    fixture(reference, number_format=published_code)
                    result = audit.audit_xlsx(actual, reference=reference)
                    self.assertFalse(result["passed"], "A format-code difference without explicit source evidence must fail")

    def test_native_external_cache_values_are_independently_inventoried(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actual.xlsx"
            fixture(path)
            add_external_link(path, "Otis-10-K-Raw.xls")
            with ZipFile(path) as archive: parts = {name: archive.read(name) for name in archive.namelist()}
            root = ET.fromstring(parts["xl/externalLinks/externalLink1.xml"])
            data = ET.SubElement(root.find("m:externalBook", NS), "{" + NS["m"] + "}sheetDataSet")
            sheet = ET.SubElement(data, "{" + NS["m"] + "}sheetData", {"sheetId": "0"})
            row = ET.SubElement(sheet, "{" + NS["m"] + "}row", {"r": "8"})
            cell = ET.SubElement(row, "{" + NS["m"] + "}cell", {"r": "G8"})
            ET.SubElement(cell, "{" + NS["m"] + "}v").text = "5468"
            parts["xl/externalLinks/externalLink1.xml"] = ET.tostring(root)
            with ZipFile(path, "w") as archive:
                for name, contents in parts.items(): archive.writestr(name, contents)
            link = audit.read_xlsx(path)["externalLinks"][0]
            self.assertEqual(link.get("cachedCells", {}).get("operations!G8"), 5468, "Native external-link cached source data must be read independently from worksheet result caches")

    def test_native_external_cache_corruption_fails_against_retained_biff(self):
        model = {"externalLinks": [{"index": 1, "sheetNames": ["operations"], "cachedCells": {"operations!G8": 5469}}]}
        source = {"sheets": [{"name": "operations", "cells": {"G8": {"type": 2, "value": 5468}}}]}
        self.assertFalse(audit.audit_external_caches(model, source)["passed"], "A corrupt external source cache must fail even when the model result cache happens to match")
        model["externalLinks"][0]["cachedCells"]["operations!G8"] = 5468
        result = audit.audit_external_caches(model, source)
        self.assertTrue(result["passed"])
        self.assertEqual(result["matchedCachedValues"], 1)


if __name__ == "__main__":
    unittest.main()
