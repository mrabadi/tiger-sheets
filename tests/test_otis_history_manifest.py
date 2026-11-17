import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SourceFirstFormatManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        folder = Path(cls.temporary.name)
        output = folder / "history.json"
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(folder / "work"), "--through", "548", "--output", str(output)], check=True, capture_output=True)
        cls.manifest = json.loads(output.read_text())
        cls.modal = next(step for step in cls.manifest["steps"] if step["action"]["type"] == "modal" and step["evidence"].get("eventId") == "initial-056")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_source_modal_lifespan_uses_observed_open_and_acceptance(self):
        self.assertEqual(self.modal["videoTime"], "08:59.400")
        self.assertEqual(self.modal["action"]["steps"][-1]["videoTime"], "09:07.167")

    def test_source_type_field_keeps_all_seven_visible_prefixes(self):
        children = self.modal["action"]["steps"]
        typed = [step for step in children if step["action"]["type"] == "text"]
        self.assertEqual([step["action"]["text"] for step in typed], ['"', 'F', 'Y', '"', ' ', 'y', 'y'])
        self.assertEqual([step["videoTime"] for step in typed], ["09:04.267", "09:04.567", "09:04.833", "09:05.367", "09:05.800", "09:06.067", "09:06.200"])

    def test_source_raw_code_does_not_invent_the_published_space_escape(self):
        typed = [step["action"]["text"] for step in self.modal["action"]["steps"] if step["action"]["type"] == "text"]
        self.assertEqual("".join(typed), '"FY" yy')

    def test_visible_s6_pointer_selection_does_not_scroll_the_merged_span(self):
        actions = [step for step in self.manifest["steps"] if step["evidence"].get("eventId") == "initial-033"]
        self.assertEqual(len(actions), 1, "The source pointer selects visible S6; a Name Box triplet scrolls its clipped merged span")
        self.assertEqual(actions[0]["action"], {"type": "mouse_cell", "address": "S6", "gesture": "click"})
        self.assertEqual(actions[0]["expect"], {"row": 6, "column": 19, "firstColumn": 1})

    def test_raw_ribbon_is_collapsed_before_the_first_observed_sheet_traversal(self):
        steps = self.manifest["steps"]
        first_navigation = next(index for index, step in enumerate(steps) if step["id"] == "intro-raw-sheet-001")
        collapse = [(index, step) for index, step in enumerate(steps) if step["action"].get("keys") == ["Ctrl+F1"] and step["evidence"].get("observed", "").startswith("The source raw workbook")]
        self.assertEqual(len(collapse), 1, "Configure the already-collapsed source workbook once")
        index, step = collapse[0]
        self.assertLess(index, first_navigation, "The original Raw view is already collapsed at06:37, before all38tab transitions")
        self.assertEqual(step["videoTime"], "06:37.000")
        self.assertEqual(step["action"], {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Ctrl+F1"]})
        self.assertEqual(step["evidence"]["kind"], "acceptance", "The physical collapse gesture is unobserved preparation")
        self.assertIn("unobserved", step["evidence"]["observed"])


class HistoricalViewportManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        folder = Path(cls.temporary.name)
        output = folder / "history.json"
        subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(folder / "work"), "--output", str(output)], check=True, capture_output=True)
        cls.manifest = json.loads(output.read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_observed_scroll_only_states_use_native_scrolls_without_reselecting(self):
        expected = [("history-0037", "09:47.000", 4), ("history-0044", "09:52.500", 7), ("history-0062", "10:05.500", 1), ("history-0392", "15:38.000", 10)]
        for event, time, row in expected:
            with self.subTest(source_event=event):
                records = [step for step in self.manifest["steps"] if step["evidence"].get("eventId") == event and step["action"]["type"] == "vertical_view"]
                self.assertEqual(len(records), 1, "The source viewport change must have a real native scroll counterpart")
                self.assertEqual(records[0]["videoTime"], time)
                self.assertEqual(records[0]["action"], {"type": "vertical_view", "topRow": row})
                self.assertEqual(records[0]["expect"]["topRow"], row)

    def test_source_uniform_external_sum_keeps_its_grouped_number_format(self):
        inspection = [step for step in self.manifest["steps"] if step["id"] == "historical-01621-keys" and step["videoTime"] == "18:50.433" and step["action"] == {"type": "key_sequence", "target": "worksheet.grid", "keys": ["F2"]}]
        self.assertEqual(len(inspection), 1, "Keep the existing source C30 inspection, without an additional selection or gesture")
        c29 = [cell for cell in inspection[0].get("expect", {}).get("cells", []) if cell.get("address") == "C29"]
        self.assertEqual(len(c29), 1, "Keep one unambiguous C29 guard in the existing source inspection")
        self.assertEqual(c29[0].get("style"), {"numberFormat": "#,##0"}, "Source18:50.5 shows C29 as1,546; its three numeric source cells all use the retainedRaw #,##0 format")
        self.assertEqual((c29[0].get("value"), c29[0].get("valueType"), c29[0].get("display")), (1546, "number", "1,546"), "The source-backed grouping guard also waits for the actual calculated SUM and display")

    def test_all_viewport_actions_keep_source_time_order(self):
        records = self.manifest["steps"]
        times = [sum(float(part) * 60 ** index for index, part in enumerate(reversed(step["videoTime"].split(":")))) for step in records]
        self.assertEqual(times, sorted(times), "A source-only scroll must execute between its neighboring worksheet actions")

    def test_original_frames_override_coarse_viewport_transcription_errors(self):
        expected = [("history-0340", "14:38.500", 4), ("history-0366", "15:03.000", 3), ("history-0600", "19:12.500", 10), ("history-0600", "19:13.000", 16), ("history-0719", "21:27.000", 31), ("history-0771", "22:26.000", 16), ("history-0771", "22:26.500", 19), ("history-1102", "29:11.000", 49), ("history-1302", "33:11.500", 31), ("history-1397", "34:48.000", 1), ("history-1398", "34:51.500", 52)]
        for event, time, row in expected:
            with self.subTest(source_event=event, time=time):
                records = [step for step in self.manifest["steps"] if step["evidence"].get("eventId") == event and step["videoTime"] == time and step["action"]["type"] == "vertical_view"]
                self.assertEqual(len(records), 1, "Every corrected full source-frame state retains an individual scroll/checkpoint")
                self.assertEqual(records[0]["action"]["topRow"], row)

    def test_source_liabilities_header_retains_its_observed_typo(self):
        commits = [cell for step in self.manifest["steps"] for cell in step.get("expect", {}).get("cells", []) if cell["address"] == "B35" and "input" in cell]
        self.assertEqual([cell["input"] for cell in commits], ["LIABILITES & EQUITY:"], "20:51 original formula bar visibly omits the second I; the final file must keep the observed source text")

    def test_clipped_source_rows_keep_the_measured_pixel_offset(self):
        for event, time, row in [("history-1005", "26:36.000", 8), ("history-1092", "28:52.500", 48)]:
            with self.subTest(source_event=event):
                record = next(step for step in self.manifest["steps"] if step["evidence"].get("eventId") == event and step["videoTime"] == time and step["action"]["type"] == "vertical_view")
                self.assertEqual(record["action"], {"type": "vertical_view", "topRow": row, "topRowOffset": -13}, "Original model rows are21px; the first row has only8px visible below the166px body edge")
                self.assertEqual(record["expect"], {"topRow": row, "topRowOffset": -13})

    def test_local_balance_formulas_keep_the_observed_wrong_pointer_corrections(self):
        expected = [("23:33.000", "D44"), ("23:33.500", "D42"), ("23:34.000", "D46"), ("23:45.000", "D48"), ("23:45.500", "D44"), ("23:46.000", "D33"), ("23:47.000", "D48")]
        for time, address in expected:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": address, "gesture": "click"} for step in self.manifest["steps"]), "Source-visible wrong and corrected pointer must execute, not just the final formula: " + time + " " + address)

    def test_source_equity_label_typing_keeps_lowercase_mistake_and_late_commit(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "21:29.000" and step["action"].get("text") == " equ" for step in steps), "The lowercase source mistake must be entered into the actual editor")
        self.assertTrue(any(step["videoTime"] == "21:30.000" and step["action"].get("keys") == ["Backspace"] * 3 for step in steps), "The source correction removes the three lowercase letters before retyping")
        commits = [step for step in steps if any(cell.get("address") == "B44" and cell.get("input") == "Common Shareholders' Equity:" for cell in step.get("expect", {}).get("cells", []))]
        self.assertEqual([step["videoTime"] for step in commits], ["21:30.933"])

    def test_source_switch_and_held_viewports_are_explicit_too(self):
        for event, time, row in [("history-0498", "17:26.000", 7), ("history-0515", "17:45.000", 16), ("history-0952", "25:20.500", 4), ("history-0997", "26:29.000", 43), ("history-1076", "28:14.000", 4), ("history-1116", "29:45.500", 55), ("history-1412", "35:10.000", 1)]:
            self.assertTrue(any(step["videoTime"] == time and step["evidence"].get("eventId") == event and step["action"] == {"type": "vertical_view", "topRow": row} for step in self.manifest["steps"]), "Explicit held/source-switch viewport needs a native state/checkpoint: " + event)

    def test_first_half_manual_entries_use_source_phases_and_preexisting_bold(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "10:29.500" and step["action"].get("text") == "Operating inc" for step in steps), "B11 manual block must retain the source's lowercase intermediate correction")
        self.assertTrue(any(step["videoTime"] == "18:08.000" and step["action"].get("keys") == ["Backspace"] for step in steps), "B33 TOT-to-TO correction must occur at the actual source time")
        self.assertTrue(any(step["videoTime"] == "17:46.500" and step["action"].get("keys") == ["Ctrl+B"] for step in steps), "Source-bold ASSETS must be bold before the actual first AS prefix")

    def test_first_source_unmerge_modal_children_keep_their_own_times(self):
        record = next(step for step in self.manifest["steps"] if step["action"]["type"] == "modal" and step["evidence"].get("eventId") == "history-0005")
        children = record["action"]["steps"]
        self.assertEqual(next(step["videoTime"] for step in children if step["action"].get("tab") == "Alignment"), "09:26.500")
        self.assertEqual(next(step["videoTime"] for step in children if step["action"].get("target") == "formatCells.merge"), "09:29.000")
        self.assertEqual(children[-1]["videoTime"], "09:30.500")

    def test_first_half_local_sum_corrections_use_real_pointer_ranges(self):
        for time, address, gesture in [("13:12.000", "C10", "press"), ("13:12.500", "C8", "move"), ("13:12.500", "C8", "release"), ("13:16.500", "C13", "click"), ("13:17.000", "C11", "click")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": address, "gesture": gesture} for step in self.manifest["steps"]), "Source SUM correction must point at the observed actual cell/range: " + time)

    def test_dense_text_and_pointing_actions_retain_their_exact_phase_frame(self):
        cases = [
            ("10:24.500", "historical-text-phases-0001-0700.json", "first-half-text-phases/0624.500.jpg"),
            ("13:12.000", "historical-local-formula-phases-0001-0700.json", "first-half-local-formulas/0792.000.jpg"),
        ]
        for time, ledger, frame in cases:
            with self.subTest(time=time):
                records = [step for step in self.manifest["steps"] if step["videoTime"] == time and step["action"]["type"] in ["text", "mouse_cell"]]
                self.assertTrue(records)
                expected = str(ROOT / "docs/validation/otis-90-minute/source-frames" / frame)
                self.assertTrue(any(step["evidence"].get("sourceFrame") == expected for step in records), "The action must point to its actual dense original frame, not the nearest coarse observation")
                self.assertTrue(any(step["evidence"].get("phaseLedger") == ledger for step in records), "Phase provenance must remain independently traceable")

    def test_second_half_drafts_keep_source_errors_and_late_commits(self):
        steps = self.manifest["steps"]
        for time, typed in [("27:32.500", "_"), ("28:37.500", "="), ("29:35.000", "die"), ("29:46.000", "?")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("text") == typed for step in steps), "The source's actual mistyped draft must exist in the native editor: " + time)
        for address, time in [("B53", "27:11.400"), ("B61", "28:56.800"), ("B67", "29:20.333"), ("B70", "29:38.733")]:
            commits = [step for step in steps if any(cell.get("address") == address and "input" in cell for cell in step.get("expect", {}).get("cells", []))]
            self.assertEqual([step["videoTime"] for step in commits], [time], "Typing must remain unfinished until the source commit")

    def test_existing_pension_label_receives_its_prefix_at_the_actual_caret(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "28:07.000" and step["action"].get("keys") == ["F2", "Home"] for step in steps), "The later source correction edits the existing label at its start")
        self.assertTrue(any(step["videoTime"] == "28:08.500" and step["action"].get("text") == "(" for step in steps))
        commits = [step for step in steps if any(cell.get("address") == "B57" and cell.get("input") == "(+/-) Pensions/Other Items:" for cell in step.get("expect", {}).get("cells", []))]
        self.assertEqual([step["videoTime"] for step in commits], ["28:09.900"])

    def test_local_cash_flow_references_and_autosums_remain_real_drafts(self):
        steps = self.manifest["steps"]
        for time, cell in [("30:05.000", "C22"), ("30:09.000", "C21")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": cell, "gesture": "click"} for step in steps))
        for time in ["12:48.500", "13:59.500", "19:04.400", "22:38.000", "23:21.867", "30:49.200", "33:13.467"]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("keys") == ["Alt+="] for step in steps), "Source AutoSUM draft must open through its real command: " + time)
        commits = [step["videoTime"] for step in steps if any(cell.get("address") == "C72" and cell.get("input") == "=SUM(C67:C71)" for cell in step.get("expect", {}).get("cells", []))]
        self.assertEqual(commits, ["33:13.733"])

    def test_late_historical_selection_and_modal_substates_have_native_actions(self):
        def all_steps(records):
            for step in records:
                yield step
                yield from all_steps(step["action"].get("steps", []))
        steps = list(all_steps(self.manifest["steps"]))
        for time, action, header, row, column in [
                ("21:13.267", {"type": "key_sequence", "target": "worksheet.grid", "keys": ["Shift+Right"], "releaseModifiers": False}, "1R x 3C", 42, 2),
                ("21:15.067", {"type": "mouse_cell", "address": "B41", "gesture": "move"}, "6R x 1C", 36, 2),
                ("21:50.000", {"type": "mouse_cell", "address": "F43", "gesture": "move"}, "2R x 5C", 42, 2)]:
            matches = [step for step in steps if step["videoTime"] == time and step["action"] == action]
            self.assertEqual(len(matches), 1, "Keep the observed held extent, including overshoot")
            self.assertEqual(matches[0]["expect"], {"row": row, "column": column, "nameBoxText": header})
        self.assertTrue(any(step["videoTime"] == "23:31.500" and step["action"].get("target") == "formula.name" and step["action"].get("text") == "D48" for step in steps))
        for time, target in [("21:21.500", "formatCells.border.bottom"), ("23:37.500", "formatCells.currencySymbol")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("target") == target for step in steps), "Each observed dialog substate must execute while the real modal remains open")

    def test_forecast_headers_use_native_f4_and_function_completion(self):
        steps = self.manifest["steps"]
        for time in ["26:33.133", "26:54.933", "26:57.867"]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("keys") == ["F4"] for step in steps), "Observed F4 anchors must cycle the actual draft rather than type its final absolute formula")
        self.assertTrue(any(step["videoTime"] == "26:38.500" and step["action"].get("text") == "EOM" for step in steps))
        self.assertTrue(any(step["videoTime"] == "26:39.000" and step["action"].get("keys") == ["Tab"] for step in steps))

    def test_cashflow_cleanup_keeps_transient_destinations_and_delayed_pastes(self):
        steps = self.manifest["steps"]
        for time, address in [("24:48.000", "J4"), ("24:48.500", "J2"), ("24:49.000", "J1"), ("26:01.000", "A53"), ("26:01.500", "A51")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("text") == address and step["action"].get("target") == "formula.name" for step in steps))
        self.assertTrue(any(step["videoTime"] == "24:49.500" and step["action"].get("keys") == ["Ctrl+V"] for step in steps))
        self.assertTrue(any(step["videoTime"] == "26:03.500" and step["action"].get("keys") == ["Ctrl+-"] for step in steps))

    def test_cf_header_uses_ordinary_paste_and_final_labels_keep_their_prefixes(self):
        steps = self.manifest["steps"]
        source_pastes = [step for step in steps if step["videoTime"] == "27:04.500" and step["evidence"].get("eventId") == "history-1039"]
        self.assertTrue(any(step["action"].get("keys") == ["Ctrl+V"] for step in source_pastes), "Source copies both rows with ordinary paste; it does not display Paste Special")
        self.assertFalse(any(step["action"]["type"] == "modal" for step in source_pastes))
        for time, text in [("33:30.000", " Rat"), ("33:33.000", " Cha")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("text") == text for step in steps))

    def test_source_tooltip_and_literal_dash_are_verified_without_mutation(self):
        steps = self.manifest["steps"]
        tooltip = next(step for step in steps if step["videoTime"] == "23:59.000" and step["action"].get("gesture") == "hover")
        self.assertEqual(tooltip.get("expect", {}).get("toolTip"), "White, Background 1, Darker 15%")
        dash = [step for step in steps if step["videoTime"] == "32:18.500"
                and any(cell.get("address") == "C37" for cell in step.get("expect", {}).get("cells", []))]
        self.assertEqual(len(dash), 1)
        self.assertEqual(dash[0]["expect"]["cells"],
                         [{"address": "C37", "input": "'—", "display": "—", "style": {"quotePrefix": False}}],
                         "The actual imported source U+2014 literal is inspected before its later numeric-zero correction")
        self.assertEqual(dash[0]["action"], {"type": "key_sequence", "target": "worksheet.grid", "keys": []})

    def test_fx_label_is_bold_while_net_cash_draft_starts_regular(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "33:29.000" and step["action"].get("keys") == ["Ctrl+B"] for step in steps), "The source FX draft is visibly bold from its first two letters")
        self.assertFalse(any(step["videoTime"] == "33:32.000" and step["action"].get("keys") == ["Ctrl+B"] for step in steps), "Net starts regular; bold appears only at the accepted result")
        commit = next(step for step in steps if step["videoTime"] == "33:34.633" and step.get("expect", {}).get("nameBoxText") == "B76")
        bold = next(step for step in steps if step["videoTime"] == "33:34.867" and step["action"].get("keys") == ["Ctrl+B"])
        regular = next(step for step in steps if step["videoTime"] == "33:35.433" and step["action"].get("keys") == ["Ctrl+B"])
        self.assertEqual(commit["action"], {"type": "key_sequence", "target": "worksheet.cellEditor", "keys": ["Ctrl+Return"]})
        self.assertEqual(commit["expect"]["cells"], [{"address": "B76", "input": "Net Change in Cash:"}])
        self.assertEqual(bold["expect"]["cells"], [{"address": "B76", "input": "Net Change in Cash:", "style": {"bold": True}}])
        self.assertEqual(regular["expect"], {"row": 74, "column": 2, "nameBoxText": "B74", "cells": [{"address": "B74", "input": "FX Rate Effects:", "style": {"bold": False}}]})
        navigation = [step for step in steps if step["videoTime"] in ["33:35.067", "33:35.267"]]
        self.assertEqual([(step["action"].get("keys"), step["expect"]["nameBoxText"]) for step in navigation], [(["Up"], "B75"), (["Up"], "B74")])
        self.assertLess(steps.index(commit), steps.index(bold))
        self.assertLess(steps.index(bold), steps.index(navigation[0]))
        self.assertLess(steps.index(navigation[-1]), steps.index(regular))

    def test_pending_investing_autosum_is_inspected_before_commit(self):
        steps = self.manifest["steps"]
        raw_windows = [i for i, step in enumerate(steps) if step["videoTime"] == "31:22.000" and step["action"].get("documentName") == "Otis-10-K-Raw.xls"]
        origin_windows = [i for i, step in enumerate(steps) if step["videoTime"] == "31:22.500" and step["action"].get("documentName") == "101-01-90-Minute-Case-Study-Otis-Blank.xlsx"]
        self.assertTrue(raw_windows, "The real source inspection follows the already committed AutoSum")
        self.assertTrue(origin_windows, "Return to the actual origin after inspecting the source")
        raw, origin = raw_windows[0], origin_windows[0]
        commit = next(i for i, step in enumerate(steps) if any(cell.get("address") == "C64" and cell.get("input") == "=SUM(C62:C63)" for cell in step.get("expect", {}).get("cells", [])))
        self.assertLess(raw, origin)
        self.assertLess(commit, raw, "Dense first Ready at31:21.033 retains C64 before the separate workbook inspection")
        self.assertEqual(steps[commit]["expect"]["nameBoxText"], "C64")

    def test_local_cash_flow_acceptance_retains_the_observed_current_cell(self):
        for time, address in [("30:05.500", "C54"), ("30:10.000", "C55")]:
            commits = [step for step in self.manifest["steps"] if step["videoTime"] == time and any(cell.get("address") == address and "input" in cell for cell in step.get("expect", {}).get("cells", []))]
            self.assertEqual(len(commits), 1)
            self.assertEqual(commits[0]["action"], {"type": "mouse", "target": "formula.accept", "gesture": "click"}, "Original Ready-state NameBox retains the accepted source cell")

    def test_corrected_external_signs_keep_the_open_edit_and_late_commit(self):
        steps = self.manifest["steps"]
        for time in ["13:00.000", "14:43.000", "14:46.000"]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("keys") == ["F2"] for step in steps), "The actual original positive reference is reopened before sign insertion")
        for time in ["13:01.500", "14:44.500", "14:47.567"]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("text") == "-" and step["action"].get("target") == "worksheet.cellEditor" for step in steps))
        for address, time in [("C18", "13:02.067"), ("C14", "14:45.500"), ("C15", "14:47.800")]:
            self.assertTrue(any(step["videoTime"] == time and any(cell.get("address") == address and str(cell.get("input", "")).startswith("=-'") for cell in step.get("expect", {}).get("cells", [])) for step in steps))

    def test_corrected_expense_signs_retain_source_ready_selection(self):
        self.assertTrue(any(step["videoTime"] == "13:02.500" and step["action"].get("target") == "formula.name" and step["action"].get("text") == "C20" for step in self.manifest["steps"]), "Source tax correction has already moved to C20 before its next draft begins")
        for address, time, row in [("C14", "14:45.500", 14), ("C15", "14:47.800", 15)]:
            commit = next(step for step in self.manifest["steps"] if step["videoTime"] == time and any(cell.get("address") == address and "input" in cell for cell in step.get("expect", {}).get("cells", [])))
            self.assertEqual(commit["action"], {"type": "mouse", "target": "formula.accept", "gesture": "click"}, "The observed Ready state retains the corrected cell; native checkmark adapts an unobservable physical acceptance gesture")
            self.assertEqual((commit["expect"]["row"], commit["expect"]["column"]), (row, 3))

    def test_absolute_receivable_reference_is_cycled_with_actual_f4(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "19:52.000" and step["action"].get("keys") == ["F4"] for step in steps))
        self.assertTrue(any(step["videoTime"] == "19:52.500" and step["action"].get("keys") == ["F4", "F4"] for step in steps), "The remaining two cycles produce the observed fully relative B9 instead of typing a replacement formula")

    def test_nci_reference_stays_positive_until_the_observed_late_sign_edit(self):
        steps = self.manifest["steps"]
        early = [step for step in steps if step["videoTime"] == "13:38.000"]
        self.assertFalse(any(step["action"].get("text") == "-" for step in early), "Dense original frames show a bare equals draft, not an early negative prefix")
        self.assertTrue(any(step["videoTime"] == "13:44.000" and step["action"].get("text") == "-" for step in steps), "Only the observed late caret edit inserts the minus sign")
        commit = next(step for step in steps if step["videoTime"] == "13:44.500" and any(cell.get("address") == "C21" and cell.get("input") == "=-'[Otis-10-K-Raw.xls]operations'!G24" for cell in step.get("expect", {}).get("cells", [])))
        self.assertEqual(commit["action"].get("keys"), ["Shift+Return"], "Source Ready state has already returned upward to C20")
        self.assertEqual((commit["expect"]["row"], commit["expect"]["column"]), (20, 3))

    def test_returned_origin_formulas_enter_the_observed_inline_edit_state(self):
        self.assertTrue(any(step["videoTime"] == "13:42.000" and step["action"].get("target") == "formula.input" and step["action"].get("keys") == ["F2"] for step in self.manifest["steps"]), "The unfinished returned Point draft resumes through its formula-bar surface")
        initial = next(step for step in self.manifest["steps"] if step["id"] == "historical-early-d41-initial-commit")
        self.assertEqual((initial["videoTime"], initial["expect"]["nameBoxText"]), ("22:35.600", "D41"), "D41 is first committed, then separately inspected in Edit")
        inspection = next(step for step in self.manifest["steps"] if step["videoTime"] == "22:35.900" and step["action"].get("keys") == ["F2"])
        self.assertEqual(inspection["action"], {"type": "key_sequence", "target": "worksheet.grid", "keys": ["F2"]}, "F2 starts a new inline inspection from the already committed D41 cell")
        self.assertEqual(inspection["expect"], {"row": 41, "column": 4, "nameBoxText": "SUM", "formulaDraft": initial["expect"]["cells"][0]["input"]}, "The observed inline inspection keeps its D41 origin and complete formula draft")

    def test_legacy_width_banner_precedes_the_cashflow_width_dialog(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "25:08.000" and step["action"].get("keys") == ["Alt", "O"] for step in steps), "The observed legacy Office access-key banner must be visible before the modal")
        dialog = next(step for step in steps if step["videoTime"] == "25:08.500" and step["action"]["type"] == "modal")
        self.assertEqual(dialog["action"]["open"]["keys"], ["C", "W"], "Continue the actual already-open legacy sequence without restarting it")
        self.assertFalse(any(step["videoTime"] == "25:08.500" and step["action"].get("target") == "formula.name" for step in steps), "Name Box focus must not interrupt the retained native legacy banner")

    def test_observed_inline_point_and_enter_surfaces_use_real_controls(self):
        steps = self.manifest["steps"]
        for time, address in [("12:59.000", "C18"), ("18:28.500", "C28")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": address, "gesture": "click"} for step in steps), "A real original-cell pointer restores the observed inline Point surface")
        for target in ["formula.input", "worksheet.cellEditor"]:
            self.assertTrue(any(step["videoTime"] == "12:20.750" and step["action"].get("keys") == ["F2"] and step["action"].get("target") == target for step in steps), "Two real F2 controls restore the observed inline Enter surface")
        self.assertTrue(any(step["videoTime"] == "12:23.500" and step["action"].get("target") == "formula.accept" for step in steps), "Source OpEx acceptance retains C10")
        self.assertTrue(any(step["videoTime"] == "12:23.500" and step["action"].get("keys") == ["Ctrl+C"] for step in steps), "The source's visible marching-ants copy outline has a genuine native Copy counterpart")

    def test_initial_gridline_peek_and_legacy_width_keep_separate_source_states(self):
        steps = self.manifest["steps"]
        for time, keys in [("08:11.000", ["Alt", "W"]), ("08:12.500", ["Escape"]), ("08:13.000", ["Alt", "W", "V", "G"]), ("08:14.300", ["Alt", "O"])]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("keys") == keys for step in steps), "The observed View peek, collapse, gridline change and legacy banner must remain separate: " + time)
        dialog = next(step for step in steps if step["videoTime"] == "08:15.000" and step["action"]["type"] == "modal")
        self.assertEqual(dialog["action"]["open"]["keys"], ["C", "W"])
        self.assertEqual([(step["videoTime"], step["action"].get("text")) for step in dialog["action"]["steps"] if step["action"]["type"] == "text"], [("08:16.000", "2")])
        self.assertEqual(dialog["action"]["steps"][-1]["videoTime"], "08:16.500")
        self.assertEqual((dialog["expect"]["row"], dialog["expect"]["column"]), (4, 1), "Source Column Width preserves A4, rather than navigating to A1")

    def test_initial_title_uses_dense_observed_prefixes_and_late_acceptance(self):
        steps = self.manifest["steps"]
        for time, typed in [("08:22.533", "I"), ("08:22.667", "n"), ("08:22.700", "c"), ("08:23.733", "e"), ("08:24.467", ":")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("text") == typed for step in steps), "The initial title must retain its actual30fps prefix: " + time)
        self.assertFalse(any(step["videoTime"] == "08:19.000" and step["action"].get("text") == "I" for step in steps))
        commit = next(step for step in steps if any(cell.get("address") == "B2" and cell.get("input") == "Income Statement:" for cell in step.get("expect", {}).get("cells", [])))
        self.assertEqual(commit["videoTime"], "08:24.600")
        self.assertEqual(commit["action"].get("target"), "formula.accept")

    def test_initial_whole_sheet_font_edit_retains_selected_then_typed_then_applied(self):
        for time, field, value in [("08:27.000", "keys", ["Ctrl+A"]), ("08:28.000", "text", "12"), ("08:28.500", "keys", ["Return"])]:
            self.assertTrue(any(step["videoTime"] == time and step["action"].get("target") == "tutorial.qat.fontSize" and step["action"].get(field) == value for step in self.manifest["steps"]), "Font selection, replacement and application must be separate observed source states")

    def test_initial_date_copy_and_formats_acceptance_keep_their_source_times(self):
        steps = self.manifest["steps"]
        self.assertTrue(any(step["videoTime"] == "08:39.000" and step["action"].get("text") == "1" and step["action"].get("target") == "worksheet.grid" for step in steps))
        self.assertTrue(any(step["videoTime"] == "08:49.000" and step["action"].get("keys") == ["Ctrl+C"] for step in steps))
        dialog = next(step for step in steps if step["videoTime"] == "08:50.000" and step["action"]["type"] == "modal")
        self.assertEqual(dialog["action"]["steps"][-1]["videoTime"], "08:51.000", "The source keeps Paste Special visible until its later acceptance")

    def test_raw_fixture_is_open_by_the_observed_source_activation(self):
        modal = next(step for step in self.manifest["steps"] if step["action"]["type"] == "modal" and any(child["action"].get("target") == "fileNameEdit" and str(child["action"].get("text", "")).endswith("Otis-10-K-Raw.xls") for child in step["action"]["steps"]))
        self.assertEqual(modal["videoTime"], "06:37.000", "Source already shows the raw workbook at397; disclosed native preparation cannot leave a false blank workbook until400")
        self.assertEqual(modal["evidence"]["kind"], "acceptance", "Opening the already-present source is fixture preparation, not an invented observed video operation")

    def test_intro_raw_drag_headers_use_real_pointer_phases(self):
        steps = self.manifest["steps"]
        expected = [("07:03.767", "A2", "press"), ("07:03.900", "F4", "move"), ("07:03.967", "F7", "move"), ("07:04.033", "F8", "move"), ("07:04.067", "I8", "move"), ("07:04.467", "I8", "release"), ("07:06.500", "G8", "press"), ("07:06.633", "I12", "move"), ("07:06.667", "I16", "move"), ("07:06.700", "I20", "move"), ("07:06.733", "I22", "move"), ("07:06.767", "I23", "move"), ("07:06.933", "I23", "release")]
        for time, address, gesture in expected:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": address, "gesture": gesture} for step in steps), "The observed live row/column dimension header needs an actual held-pointer phase: " + time)

    def test_intro_repaint_residue_does_not_create_phantom_selections(self):
        steps = self.manifest["steps"]
        self.assertFalse(any(step["action"].get("target") == "formula.name" and step["action"].get("text") in ["A4:H9", "G14:H18"] for step in steps), "Dense source frames prove these coarse range readings are partial repaints during clicks, rather than actual additional selections")
        for time, address in [("07:05.200", "D12"), ("07:07.700", "G16")]:
            self.assertTrue(any(step["videoTime"] == time and step["action"] == {"type": "mouse_cell", "address": address, "gesture": "click"} for step in steps))
        scroll = next(step for step in steps if step["videoTime"] == "07:07.333" and step["action"]["type"] == "vertical_view")
        self.assertEqual(scroll["action"]["topRow"], 4)
        self.assertEqual((scroll["expect"]["row"], scroll["expect"]["column"]), (8, 7))

    def test_canonical_intro_drag_asserts_anchor_and_formula_during_each_move(self):
        for time, row, column, formula in [("07:03.900", 2, 1, "CONSOLIDATED STATEMENTS OF OPERATIONS"), ("07:04.067", 2, 1, "CONSOLIDATED STATEMENTS OF OPERATIONS"), ("07:06.633", 8, 7, "6428"), ("07:06.767", 8, 7, "6428")]:
            record = next(step for step in self.manifest["steps"] if step["videoTime"] == time and step["action"]["type"] == "mouse_cell")
            self.assertEqual(record.get("expect"), {"row": row, "column": column, "formulaDraft": formula}, "The canonical native replay must independently verify the anchor/formula-bar contract, rather than relying only on screenshots")


class IntroWindowGeometryManifestTests(unittest.TestCase):
    def test_inspected_complete_workbook_stays_open_for_later_source_switches(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            output = folder / "intro.json"
            subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(folder / "work"), "--include-intro", "--through", "1867", "--output", str(output)], check=True, capture_output=True)
            steps = json.loads(output.read_text())["steps"]
            self.assertFalse(any(step["videoTime"] == "00:27.000" and step["action"].get("keys") == ["Ctrl+W"] for step in steps), "The source's later switcher proves Complete is retained")
            self.assertTrue(any(step.get("expect", {}).get("windowCount") == 3 for step in steps))
            self.assertTrue(any(step["videoTime"] == "31:04.033" and step["action"].get("keys") == ["Ctrl+Tab"] for step in steps), "The first attempted Complete activation remains a genuine three-window action")
            self.assertTrue(any(step["videoTime"] == "31:04.100" and step["action"].get("keys") == ["Ctrl+Shift+Tab"] for step in steps), "Return to origin preserves the unfinished formula without fabricated NameBox text")

    def test_source_window_drag_states_have_native_geometry_counterparts(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            output = folder / "intro.json"
            subprocess.run([sys.executable, str(ROOT / "tools/build_otis_history_manifest.py"), "--workspace", str(folder / "work"), "--include-intro", "--through", "21.5", "--output", str(output)], check=True, capture_output=True)
            steps = json.loads(output.read_text())["steps"]
            geometry = [step for step in steps if step["action"]["type"] == "window_geometry"]
            self.assertEqual([(step["videoTime"], step["action"]["rect"]) for step in geometry], [("00:20.000", [1103, 94, 1280, 720]), ("00:20.500", [105, 66, 1280, 720]), ("00:21.000", [-18, 10, 1280, 720]), ("00:21.500", [0, 0, 1280, 720])])
            self.assertTrue(all("outer" in step["evidence"].get("replayMethod", "").lower() for step in geometry), "Original Windows outer borders versus Qt client geometry must remain an explicit adaptation")


if __name__ == "__main__":
    unittest.main()
