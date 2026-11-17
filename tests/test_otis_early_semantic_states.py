"""Source-defined intermediate states, independent of helper event aliases."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def flatten(records):
    for record in records:
        yield record
        yield from flatten(record["action"].get("steps", []))


class EarlyHistoricalSemanticStates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        supplied = os.environ.get("OTIS_SEMANTIC_MANIFEST")
        cls.temporary = tempfile.TemporaryDirectory()
        path = Path(supplied) if supplied else Path(cls.temporary.name) / "manifest.json"
        if not supplied:
            subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(Path(cls.temporary.name) / "work"), "--output", str(path)], check=True, capture_output=True)
        cls.manifest = json.loads(path.read_text())
        cls.steps = list(flatten(cls.manifest["steps"]))

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def action(self, time, action, event=None):
        found = [r for r in self.steps if r["videoTime"] == time and r["action"] == action and (event is None or r["evidence"].get("eventId") == event)]
        self.assertEqual(len(found), 1, f"One actual source-state action is required: {time} {event} {action}")
        return found[0]

    def selection(self, time, address, event=None):
        return self.action(time, {"type": "text", "target": "formula.name", "text": address}, event)

    def test_first_unmerge_retains_alignment_checkbox_and_acceptance_phases(self):
        self.selection("09:25.500", "G1:I1048576", "history-0004")
        self.action("09:26.500", {"type": "mouse_tab", "target": "formatCells.tabs", "tab": "Alignment"}, "history-0006")
        self.action("09:29.000", {"type": "mouse", "target": "formatCells.merge", "gesture": "click"}, "history-0007")
        self.action("09:30.500", {"type": "key_sequence", "target": "formatCells.dialog", "keys": ["Return"]}, "history-0008")

    def test_column_cut_destination_is_not_pasted_before_it_is_selected(self):
        self.selection("09:39.000", "G1", "history-0022")
        self.selection("09:40.000", "K1", "history-0024")
        self.action("09:40.500", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+V"]}, "history-0025")

    def test_short_lived_single_cell_precedes_the_indent_range(self):
        self.action("11:18.500", {"type":"mouse_cell","address":"B13","gesture":"click"}, "history-0131")
        held=self.action("11:18.900", {"type":"key_sequence","target":"worksheet.grid","keys":["Shift+Down"],"releaseModifiers":False})
        self.assertEqual(held["expect"]["nameBoxText"],"3R x 1C")
        self.action("11:20.000", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Alt", "H", "6"]})

    def test_border_popup_highlight_and_commit_are_separate_states(self):
        self.action("11:27.500", {"type": "mouse", "target": "tutorial.qat.borders", "gesture": "click"}, "history-0144")
        self.action("11:27.500", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Down"]}, "history-0144")
        self.action("11:28.000", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Alt"]}, "history-0145")
        self.action("11:28.500", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Down"]}, "history-0146")
        self.action("11:29.500", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Return"]}, "history-0147")

    def test_second_border_popup_retains_bottom_highlight_until_top_command(self):
        self.action("13:56.500", {"type": "mouse", "target": "tutorial.qat.borders", "gesture": "click"}, "history-0309")
        self.action("13:56.500", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Down"]}, "history-0309")
        self.action("13:57.000", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Alt"]}, "history-0310")
        self.action("13:58.000", {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["P"]}, "history-0311")
        self.assertFalse(any(r["videoTime"] in {"13:57.000", "13:57.500"} and r["action"] == {"type": "key_sequence", "target": "tutorial.borderMenu", "keys": ["Down"]} for r in self.steps), "The source still highlights Bottom at13:57.5; do not invent an earlier Top highlight")

    def test_border_none_and_outline_hover_keep_preview_unmodified(self):
        self.action("11:36.000", {"type": "mouse", "target": "formatCells.borderPreset.none", "gesture": "hover"}, "history-0158")
        self.action("11:36.500", {"type": "mouse", "target": "formatCells.borderPreset.outline", "gesture": "hover"}, "history-0159")

    def test_formula_paste_acceptance_follows_the_visible_modal(self):
        self.action("12:16.000", {"type": "key_sequence", "target": "pasteSpecial.dialog", "keys": ["Return"]}, "history-0202")
        release = self.action("12:47.633", {"type": "modifier_release", "target": "pasteSpecial.dialog", "keys": ["Shift"]}, "history-0241")
        accepted = self.action("12:47.633", {"type": "key_sequence", "target": "pasteSpecial.dialog", "keys": ["Return"]}, "history-0241")
        next_cell = self.action("12:47.833", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Tab"]})
        copy_end = self.action("12:47.967", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Escape"]})
        self.assertEqual(release["expect"]["nameBoxText"], "C14")
        self.assertEqual(next_cell["expect"], {"row": 15, "column": 3, "nameBoxText": "C15"})
        self.assertEqual(copy_end["expect"], next_cell["expect"])
        positions = [self.steps.index(r) for r in [release, accepted, next_cell, copy_end]]
        self.assertEqual(positions, sorted(set(positions)))
        self.selection("12:16.000", "C10", "history-0202")

    def test_assets_comparison_returns_to_the_model_before_its_next_selection(self):
        self.action("19:45.500", {"type": "activate_window", "documentName": "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"}, "history-0630")
        self.selection("19:47.500", "E27", "history-0631")
        self.action("19:55.500", {"type": "activate_window", "documentName": "Otis-10-K-Raw.xls"}, "history-0642")

    def test_accounting_dialog_is_not_collapsed_to_its_opening_time(self):
        held=self.action("12:28.367", {"type":"mouse_cell","address":"E11","gesture":"move"}, "history-0215")
        self.assertEqual(held["expect"]["nameBoxText"],"4R x 3C")
        self.action("12:30.000", {"type": "mouse_tab", "target": "formatCells.tabs", "tab": "Number"}, "history-0217")
        self.action("12:30.500", {"type": "key_sequence", "target": "formatCells.categoryList", "keys": ["Home", "Down", "Down", "Down"]}, "history-0218")
        self.action("12:31.500", {"type": "text", "target": "formatCells.decimalPlaces", "text": "0"}, "history-0219")
        self.action("12:32.000", {"type": "key_sequence", "target": "formatCells.dialog", "keys": ["Return"]}, "history-0220")

    def test_cost_format_source_is_copied_before_the_single_paste_anchor(self):
        phases = []
        for time, address, gesture, header in [
                ("12:49.933", "C8", "press", "C8"),
                ("12:50.033", "C9", "move", "2R x 1C"),
                ("12:50.100", "D10", "move", "3R x 2C"),
                ("12:50.167", "E11", "move", "4R x 3C"),
                ("12:50.533", "E11", "release", "C8")]:
            record = self.action(time, {"type": "mouse_cell", "address": address, "gesture": gesture})
            self.assertEqual(record["expect"], {"row": 8, "column": 3, "nameBoxText": header})
            phases.append(record)
        copied = self.action("12:51.000", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+C"]}, "history-0246")
        destination = self.selection("12:51.000", "C13", "history-0246")
        positions = [self.steps.index(r) for r in phases + [copied, destination]]
        self.assertEqual(positions, sorted(set(positions)))
        self.assertFalse(any(r["action"] == {"type": "text", "target": "formula.name", "text": "C8:E11"}
                             for r in self.steps if "12:49.933" <= r["videoTime"] <= "12:50.533"))

    def test_balance_header_format_destination_is_selected_before_its_modal(self):
        destination = self.action("15:42.033", {"type": "mouse_cell", "address": "C24", "gesture": "click"}, "history-0399")
        modal = next(r for r in self.manifest["steps"] if r["videoTime"] == "15:42.500" and r["action"]["type"] == "modal" and r["evidence"].get("eventId") == "history-0399")
        self.assertLess(self.manifest["steps"].index(destination), self.manifest["steps"].index(modal), "The source C24 destination must be active before Paste Special opens, not after it returns")
        self.assertEqual(destination["expect"], {"row": 24, "column": 3, "nameBoxText": "C24"})
        self.assertEqual(modal["expect"]["cells"], [{"address": col+"24", "input": "="+col+"2", "style": {"bold": True, "alignment": "center"}} for col in "CDE"], "Native acceptance must verify the actual three pasted formats and preserve the date formulas")

    def test_clipboard_warning_dismissal_is_a_later_modal_child(self):
        self.action("13:48.000", {"type": "key_sequence", "target": "@active_modal", "keys": []}, "history-0295")
        self.action("13:48.500", {"type": "key_sequence", "target": "@active_modal", "keys": ["Return"]}, "history-0296")

    def test_symbol_dropdown_is_actually_opened_before_the_dollar_choice(self):
        self.action("15:01.000", {"type": "mouse", "target": "formatCells.currencySymbol", "gesture": "click"}, "history-0363")
        self.action("15:02.000", {"type": "key_sequence", "target": "formatCells.currencySymbol", "keys": ["Home", "Down", "Return"]}, "history-0364")
        self.action("15:02.500", {"type": "key_sequence", "target": "formatCells.dialog", "keys": ["Return"]}, "history-0365")

    def test_cash_flow_merge_checkbox_off_on_off_are_distinct(self):
        for time, event in [("17:15.000", "history-0491"), ("17:15.500", "history-0492"), ("17:16.000", "history-0493")]:
            self.action(time, {"type": "mouse", "target": "formatCells.merge", "gesture": "click"}, event)

    def _assert_asset_format_copy_staging(self):
        source = self.selection("19:11.500", "C13", "history-0598")
        copied = self.action("19:12.000", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+C"]}, "history-0599")
        destination = self.selection("19:12.500", "C27", "history-0600")
        self.assertFalse(any(r["videoTime"] == "19:14.000" and r["action"] == {"type": "text", "target": "formula.name", "text": "C27:C32"} for r in self.steps), "A phantom typed NameBox range must not replace the source's held6R x1C state")
        anchor = self.action("19:14.000", {"type": "mouse_cell", "address": "C27", "gesture": "click"}, "history-0602")
        held = self.action("19:14.000", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Shift+Down"] * 5, "releaseModifiers": False}, "history-0602")
        formats = self.action("19:14.500", {"type": "mouse", "target": "pasteSpecial.formats", "gesture": "click"}, "history-0602")
        released = self.action("19:15.000", {"type": "modifier_release", "target": "pasteSpecial.dialog", "keys": ["Shift"]}, "history-0603")
        accepted = self.action("19:15.000", {"type": "key_sequence", "target": "pasteSpecial.dialog", "keys": ["Return"]}, "history-0603")
        self.assertEqual(anchor.get("expect", {}).get("nameBoxText"), "C27")
        self.assertEqual(held.get("expect", {}).get("nameBoxText"), "6R x 1C", "The original19:14 frame requires the held six-row count")
        self.assertEqual(formats.get("expect", {}).get("nameBoxText"), "6R x 1C", "The visible Formats modal must retain the source count")
        self.assertEqual(released.get("expect", {}).get("nameBoxText"), "C27")
        modal = [r for r in self.steps if r["action"]["type"] == "modal" and r["evidence"].get("eventId") == "history-0602"]
        self.assertEqual(len(modal), 1)
        self.assertEqual(modal[0]["videoTime"], "19:14.500")
        self.assertEqual(modal[0]["action"]["open"], {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Alt", "E", "S"], "releaseModifiers": False})
        self.assertEqual(modal[0]["action"]["steps"], [formats, released, accepted])
        positions = [self.steps.index(r) for r in [source, copied, destination, anchor, held, modal[0], formats, released, accepted]]
        self.assertEqual(positions, sorted(set(positions)), "Copy, held selection, visible modal, release and acceptance must remain ordered")
        self.assertFalse(any(r["action"]["type"] == "modifier_release" for r in self.steps[self.steps.index(held) + 1:self.steps.index(formats)]), "The count cannot be released before the source-visible modal")
        self.assertFalse(any(r["action"] == copied["action"] for r in self.steps[self.steps.index(copied) + 1:self.steps.index(accepted)]), "The original C13 clipboard must survive destination staging without a recopy")

    def test_asset_format_copy_preserves_source_and_destination_staging(self):
        self._assert_asset_format_copy_staging()

    def test_asset_format_staging_rejects_phantom_range_and_early_release(self):
        original = self.steps
        try:
            for mutation in ["phantom-namebox", "early-release", "missing-held-count"]:
                with self.subTest(mutation=mutation):
                    self.steps = copy.deepcopy(original)
                    held = next(r for r in self.steps if r["id"] == "historical-modal-header-history-0602-held-selection")
                    if mutation == "phantom-namebox":
                        held["action"] = {"type": "text", "target": "formula.name", "text": "C27:C32"}
                    elif mutation == "early-release":
                        released = next(r for r in self.steps if r["id"] == "historical-modal-header-history-0602-release")
                        self.steps.remove(released)
                        modal_index = next(i for i, r in enumerate(self.steps) if r["action"]["type"] == "modal" and r["evidence"].get("eventId") == "history-0602")
                        self.steps.insert(modal_index, released)
                    else:
                        held["expect"].pop("nameBoxText")
                    with self.assertRaises(AssertionError):
                        self._assert_asset_format_copy_staging()
        finally:
            self.steps = original

    def test_liability_review_retains_each_wrong_and_reduced_range(self):
        phases = []
        for time, anchor, address, gesture, header in [
                ("20:07.233", "A29", "A30", "move", "2R x 1C"),
                ("20:07.367", "A29", "A31", "move", "3R x 1C"),
                ("20:07.667", "A29", "A31", "release", "A29"),
                ("20:09.000", "A30", "B32", "move", "3R x 2C"),
                ("20:09.067", "A30", "C32", "move", "3R x 3C"),
                ("20:09.633", "A30", "C32", "release", "A30"),
                ("20:10.133", "C32", "A31", "move", "2R x 3C"),
                ("20:10.267", "C32", "A30", "move", "3R x 3C"),
                ("20:10.833", "C32", "A30", "release", "C32"),
                ("20:11.733", "A29", "A31", "move", "3R x 1C"),
                ("20:11.767", "A29", "B31", "move", "3R x 2C"),
                ("20:11.867", "A29", "B30", "move", "2R x 2C"),
                ("20:12.033", "A29", "C30", "move", "2R x 3C"),
                ("20:12.200", "A29", "C29", "move", "1R x 3C"),
                ("20:12.733", "A29", "C29", "release", "A29"),
                ("20:18.533", "A43", "A40", "move", "4R x 1C"),
                ("20:18.767", "A43", "B36", "move", "8R x 2C"),
                ("20:18.833", "A43", "C36", "move", "8R x 3C"),
                ("20:18.900", "A43", "C35", "move", "9R x 3C")]:
            record = self.action(time, {"type": "mouse_cell", "address": address, "gesture": gesture})
            column = ord(anchor[0]) - ord("A") + 1
            self.assertEqual(record["expect"], {"row": int(anchor[1:]), "column": column, "nameBoxText": header})
            phases.append(record)
        positions = [self.steps.index(r) for r in phases]
        self.assertEqual(positions, sorted(set(positions)))
        self.assertFalse(any(r["videoTime"] == "20:07.000" and r["action"].get("text") == "A22:C32" for r in self.steps), "The old coarse, unobserved range must not be replayed in addition to the real source ranges")


if __name__ == "__main__":
    unittest.main()
