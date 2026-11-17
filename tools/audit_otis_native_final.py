#!/usr/bin/env python3
"""Read-only source-first audit of the workbook produced by the native replay."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import posixpath
import re
import sys
import struct
from decimal import Decimal, ROUND_HALF_UP
from urllib.parse import urlparse, unquote
from openpyxl.utils.cell import range_boundaries, get_column_letter, coordinate_from_string, column_index_from_string
from xml.etree import ElementTree as ET
from zipfile import ZipFile
import openpyxl

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.audit_otis_replica import style_value, theme_palette

MAIN = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
DOC_REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS = {"m": MAIN}


def part_target(part, target):
    return target.lstrip("/") if target.startswith("/") else posixpath.normpath(posixpath.join(posixpath.dirname(part), target))


def relationships(archive, part):
    path = posixpath.join(posixpath.dirname(part), "_rels", posixpath.basename(part) + ".rels")
    if path not in archive.namelist():
        return {}
    return {node.attrib["Id"]: dict(node.attrib) for node in ET.fromstring(archive.read(path))}


def scalar(cell, shared):
    kind = cell.get("t", "n")
    node = cell.find("m:v", NS)
    text = node.text if node is not None else None
    if kind == "inlineStr":
        return "".join(node.text or "" for node in cell.findall(".//m:is//m:t", NS))
    if text is None:
        return None
    if kind == "s":
        return shared[int(text)]
    if kind == "b":
        return text == "1"
    if kind in ("str", "e", "d"):
        return text
    try:
        return float(text)
    except ValueError:
        return text


def read_xlsx(path):
    path = Path(path)
    book = openpyxl.load_workbook(path, data_only=False, keep_links=False)
    palette = theme_palette(book)
    cells, sheets = {}, []
    style_definitions = {}
    def resolved_style(index, sheet):
        index = int(index)
        if str(index) not in style_definitions:
            probe = openpyxl.cell.cell.Cell(sheet, row=1, column=1)
            probe._style = book._cell_styles[index]
            style_definitions[str(index)] = style_value(probe, palette)
        return style_definitions[str(index)]
    with ZipFile(path) as archive:
        workbook = ET.fromstring(archive.read("xl/workbook.xml"))
        rels = relationships(archive, "xl/workbook.xml")
        links = []
        for index, ref in enumerate(workbook.findall("m:externalReferences/m:externalReference", NS), 1):
            link_part = part_target("xl/workbook.xml", rels[ref.attrib["{" + DOC_REL + "}id"]]["Target"])
            link_xml = ET.fromstring(archive.read(link_part))
            external_book = link_xml.find("m:externalBook", NS)
            link_rels = relationships(archive, link_part)
            relation = link_rels[external_book.attrib["{" + DOC_REL + "}id"]]
            uri = relation["Target"]
            decoded = unquote(uri).replace("\\", "/")
            parsed = urlparse(decoded)
            relative = not parsed.scheme and not parsed.netloc and not decoded.startswith("/") and not re.match(r"^[A-Za-z]:", decoded)
            source_sheets = [item.get("val", "") for item in external_book.findall("m:sheetNames/m:sheetName", NS)]
            cached_cells = {}
            for data in external_book.findall("m:sheetDataSet/m:sheetData", NS):
                sheet_id = int(data.get("sheetId", "0"))
                source_sheet = source_sheets[sheet_id] if 0 <= sheet_id < len(source_sheets) else "#INVALID_SHEET_" + str(sheet_id)
                for cached_cell in data.findall("m:row/m:cell", NS):
                    cached_cells[source_sheet + "!" + cached_cell.get("r", "")] = scalar(cached_cell, [])
            links.append({"index": index, "uri": uri, "name": posixpath.basename(parsed.path), "relative": bool(relative), "targetMode": relation.get("TargetMode"), "resolvedPath": str((path.parent / decoded).resolve()) if relative else decoded, "sheetNames": source_sheets, "cachedCells": cached_cells})
        def canonical(formula):
            if formula is None:
                return None
            return re.sub(r"\[(\d+)\]", lambda match: "[" + links[int(match.group(1)) - 1]["name"] + "]" if 1 <= int(match.group(1)) <= len(links) else match.group(0), formula)
        shared = []
        if "xl/sharedStrings.xml" in archive.namelist():
            shared = ["".join(node.text or "" for node in item.findall(".//m:t", NS)) for item in ET.fromstring(archive.read("xl/sharedStrings.xml"))]
        for sheet in workbook.findall("m:sheets/m:sheet", NS):
            name = sheet.attrib["name"]
            target = part_target("xl/workbook.xml", rels[sheet.attrib["{" + DOC_REL + "}id"]]["Target"])
            xml = ET.fromstring(archive.read(target))
            resolved_style(0, book[name])
            for dimension in list(xml.findall("m:cols/m:col", NS)) + list(xml.findall("m:sheetData/m:row", NS)):
                index = dimension.get("style") if dimension.tag.endswith("col") else dimension.get("s")
                if index is not None:
                    resolved_style(index, book[name])
            views = [{"attributes": dict(view.attrib), "selections": [dict(selection.attrib) for selection in view.findall("m:selection", NS)]} for view in xml.findall("m:sheetViews/m:sheetView", NS)]
            sheet_inventory = {"name": name, "part": target, "views": views, "format": dict(xml.find("m:sheetFormatPr", NS).attrib) if xml.find("m:sheetFormatPr", NS) is not None else {}, "columns": [dict(column.attrib) for column in xml.findall("m:cols/m:col", NS)], "rows": [dict(row.attrib) for row in xml.findall("m:sheetData/m:row", NS)], "merges": [merge.attrib["ref"] for merge in xml.findall("m:mergeCells/m:mergeCell", NS)]}
            sheets.append(sheet_inventory)
            for cell in xml.findall("m:sheetData/m:row/m:c", NS):
                address = cell.attrib["r"]
                formula = cell.find("m:f", NS)
                value = scalar(cell, shared)
                resolved_formula = None
                if formula is not None:
                    expanded = book[name][address].value if formula.get("t") == "shared" else "=" + (formula.text or "")
                    if not isinstance(expanded, str) or not expanded.startswith("="):
                        raise ValueError("Cannot independently expand native formula at " + name + "!" + address)
                    resolved_formula = canonical(expanded)
                style_index = cell.get("s")
                if style_index is None:
                    style_index = inherited_style_index(sheet_inventory, address)
                cells[name + "!" + address] = {"sheet": name, "address": address, "type": cell.get("t", "n"), "formula": resolved_formula, "cachedValue": value, "input": resolved_formula if formula is not None else value, "style": resolved_style(style_index, book[name])}
    formulas = [cell for cell in cells.values() if cell["formula"] is not None]
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "storedCells": len(cells), "nonemptyCells": sum(cell["input"] is not None for cell in cells.values()), "formulaCells": len(formulas), "formulaCaches": sum(cell["cachedValue"] is not None for cell in formulas), "cells": cells, "sheets": sheets, "styleDefinitions": style_definitions, "externalLinks": links, "activeTab": int(workbook.find("m:bookViews/m:workbookView", NS).get("activeTab", "0")) if workbook.find("m:bookViews/m:workbookView", NS) is not None else 0}


def inherited_style_index(sheet, address):
    column_name, row_index = coordinate_from_string(address)
    column_index = column_index_from_string(column_name)
    for row in sheet["rows"]:
        if int(row["r"]) == row_index and "s" in row and row.get("customFormat", "0") in ("1", "true"):
            return row["s"]
    for column in reversed(sheet["columns"]):
        if int(column["min"]) <= column_index <= int(column["max"]) and "style" in column:
            return column["style"]
    return "0"


def unstored_cell_style(inventory, qualified_address):
    name, address = qualified_address.rsplit("!", 1)
    sheet = next(sheet for sheet in inventory["sheets"] if sheet["name"] == name)
    return inventory["styleDefinitions"][inherited_style_index(sheet, address)]


def numeric(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def same_value(actual, expected):
    if numeric(expected):
        return numeric(actual) and math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-8)
    if expected in (None, ""):
        return actual in (None, "")
    return type(actual) is type(expected) and actual == expected


def rounded(value, places):
    return float(Decimal(str(value)).quantize(Decimal(1).scaleb(-places), rounding=ROUND_HALF_UP))


def display_format_info(code):
    sections = code.split(";")
    positive = sections[0]
    cleaned = re.sub(r'"[^"]*"', "", positive)
    cleaned = re.sub(r"\[[^]]*\]", "", cleaned)
    cleaned = re.sub(r"\\.|_.|\*.", "", cleaned)
    match = re.search(r"[0#?][0#?,]*(?:\.([0#?]+))?", cleaned)
    fraction = match.group(1) if match else None
    decimals = (len(fraction) if set(fraction) <= {"0"} else None) if fraction else (0 if match else None)
    negative = re.sub(r"_.|\*.", "", sections[1]) if len(sections) > 1 else ""
    return {"decimals": decimals, "percent": "%" in cleaned, "currency": "$" in re.sub(r"\[[^]]*\]", "", positive), "negativeParentheses": "(" in negative and ")" in negative}


def historical_formula_checks(oracle):
    """Source-entered formulas and observed relative fills, never native output.

    The retained historical-statements-visual-steps.json records the original
    worksheet entries/fills at 11:45–14:50, 18:20–23:48 and 30:03–34:16.
    The later source inserts fifty rows above these statements. Local references
    therefore move by fifty; references into the cleaned raw workbook do not.
    """
    known = {}
    for column, operations_column, cashflow_column in zip("CDE", "GHI", "CDE"):
        op = lambda address: "'[Otis-10-K-Raw.xls]operations'!" + operations_column + address
        cf = lambda address: "'[Otis-10-K-Raw.xls]cash flows'!" + cashflow_column + address
        income = {
            54: "=" + op("8"), 55: "=" + op("9"), 56: f"=SUM({column}54:{column}55)",
            58: "=-" + op("12"), 59: "=-" + op("13"), 60: "=-SUM(" + op("14") + ":" + operations_column + "15)",
            61: f"={column}56+SUM({column}58:{column}60)",
            63: "=" + op("17"), 64: "=-" + op("19"), 65: "=-" + op("20"),
            66: f"={column}61+SUM({column}63:{column}65)", 68: "=-" + op("22"),
            70: f"={column}66+{column}68", 71: "=-" + op("24"), 72: f"=SUM({column}70:{column}71)",
        }
        cashflow = {
            104: f"={column}72", 105: f"=-{column}71", 106: "=" + cf("9"),
            107: "=" + "+".join(cf(row) for row in ("10", "11", "12", "24", "25")),
            108: "=SUM(" + cf("16") + ":" + cashflow_column + "21)", 109: f"=SUM({column}104:{column}108)",
            112: "=" + cf("28"), 113: "=SUM(" + cf("29") + ":" + cashflow_column + "33)",
            114: f"=SUM({column}112:{column}113)", 117: "=SUM(" + cf("36") + ":" + cashflow_column + "39)+" + cf("41"),
            118: "=" + cf("43"), 119: "=" + cf("44"), 120: "=" + cf("45"),
            121: "=" + "+".join(cf(row) for row in ("40", "46", "47")), 122: f"=SUM({column}117:{column}121)",
            124: "=" + cf("50"), 126: f"={column}109+{column}114+{column}122+{column}124",
        }
        known.update({column + str(row): formula for row, formula in {**income, **cashflow}.items()})
    # The source corrects the first balance-sheet year from C to D at 19:34,
    # then fills D:E. The unanchored B9 correction is retained at 19:51.
    for column, source_column in zip("DE", "BC"):
        bs = lambda address: "'[Otis-10-K-Raw.xls]consolidated balance sheets'!" + source_column + address
        balance = {
            77: "=SUM(" + bs("7") + ":" + source_column + "8)", 78: "=" + bs("9"),
            79: "=SUM(" + bs("10") + ":" + source_column + "12)",
            80: "=" + "+".join(bs(row) for row in ("15", "17", "18")), 81: "=" + bs("16"),
            82: "=" + bs("14") + "+" + bs("19"), 83: f"=SUM({column}77:{column}82)",
            86: "=" + bs("23"), 87: "=" + bs("24"), 88: "=" + bs("25"),
            89: "=" + bs("22") + "+" + bs("28"), 90: "=" + bs("30"),
            91: "=" + "+".join(bs(row) for row in ("29", "31", "32")), 92: f"=SUM({column}86:{column}91)",
            94: "=" + bs("43"), 95: "=" + bs("35") + "+" + bs("44"), 96: f"=SUM({column}94:{column}95)",
            98: f"={column}92+{column}96", 100: f"={column}83-{column}98",
        }
        known.update({column + str(row): formula for row, formula in balance.items()})
    return [{"address": item["address"], "input": known[item["address"]],
             "basis": "Retained source historical entries, corrections and relative fills; final local row offset +50."}
            for item in oracle.get("independentExactHistoricalValues", []) if item["address"] in known]


def source_input_checks(oracle):
    checks = {}
    for record in historical_formula_checks(oracle) + oracle.get("directNativeCheckpoint", {}).get("expect", {}).get("cells", []) + oracle.get("exactFormulaChecks", []):
        checks.setdefault(record["address"], {}).update(record)
    return [{"address": address, **record} for address, record in checks.items()]


def audit_xlsx(workbook, reference=None, oracle=None):
    inventory = read_xlsx(workbook)
    failures = []
    oracle = oracle or {}
    sheet_name = inventory["sheets"][0]["name"]
    def fail(code, address, expected, actual):
        failures.append({"code": code, "address": address, "expected": expected, "actual": actual})
    def cell(address):
        return inventory["cells"].get(sheet_name + "!" + address, {"input": None, "cachedValue": None, "formula": None})
    for address, item in inventory["cells"].items():
        if item["formula"] is not None and item["cachedValue"] is None:
            fail("missing_formula_cache", address, "resolved native formula result", None)
        if item["type"] == "e":
            fail("formula_or_cell_error", address, "no unresolved final error", item["cachedValue"])
        if isinstance(item["cachedValue"], float) and not math.isfinite(item["cachedValue"]):
            fail("nonfinite_cache", address, "finite result", str(item["cachedValue"]))
    expected_cells = source_input_checks(oracle) + oracle.get("independentExactHistoricalValues", [])
    for expected in expected_cells:
        address = expected["address"]
        actual = cell(address)
        if "input" in expected and not same_value(actual["input"], expected["input"]):
            fail("source_input", address, expected["input"], actual["input"])
        if "value" in expected and not same_value(actual["cachedValue"], expected["value"]):
            fail("source_value", address, expected["value"], actual["cachedValue"])
        if "display" in expected:
            text = expected["display"]
            if text.endswith("%"):
                shown = float(text[:-1])
                places = len(text.split(".")[1].rstrip("%")) if "." in text else 0
                value = actual["cachedValue"]
                if not numeric(value) or rounded(value * 100, places) != shown:
                    fail("source_percentage_display", address, text, value)
            else:
                fail("unsupported_display_oracle", address, text, "Use explicit sourceRoundedDisplays numeric/precision metadata")
    for expected in oracle.get("sourceRoundedDisplays", []):
        address = expected["address"]
        value = cell(address)["cachedValue"]
        scaled = value * 100 if numeric(value) and expected.get("unit") == "percent" else value
        if not numeric(scaled) or rounded(scaled, expected.get("decimalPlaces", 0)) != expected["displayedNumber"]:
            fail("source_rounded_display", address, expected["displayedNumber"], scaled)
        info = display_format_info(cell(address).get("style", {}).get("numberFormat", "General"))
        if info["decimals"] != expected.get("decimalPlaces", 0):
            fail("source_display_precision", address, expected.get("decimalPlaces", 0), info["decimals"])
        if (expected.get("unit") == "percent") != info["percent"]:
            fail("source_display_unit", address, expected.get("unit", "number"), info)
        if "currencySymbol" in expected and bool(expected["currencySymbol"]) != info["currency"]:
            fail("source_currency_symbol", address, bool(expected["currencySymbol"]), info["currency"])
        if expected.get("negativePresentation") == "parentheses" and numeric(value) and value < 0 and not info["negativeParentheses"]:
            fail("source_negative_parentheses", address, True, info["negativeParentheses"])
        if "roundedZeroSign" in expected:
            wanted_sign = expected["roundedZeroSign"]
            actual_sign = "negative" if numeric(value) and value < 0 else "positive" if numeric(value) and value > 0 else "zero"
            if actual_sign != wanted_sign:
                fail("source_rounded_zero_sign", address, wanted_sign, actual_sign)
    if oracle.get("requireExternalSourceLink") and not inventory["externalLinks"]:
        fail("missing_external_source_link", "workbook", "retained live raw-source reference", [])
    if "expectedExternalWorkbooks" in oracle and sorted(link["name"] for link in inventory["externalLinks"]) != sorted(oracle["expectedExternalWorkbooks"]):
        fail("external_source_names", "workbook", oracle["expectedExternalWorkbooks"], [link["name"] for link in inventory["externalLinks"]])
    for link in inventory["externalLinks"]:
        if not link["relative"] or link["targetMode"] != "External":
            fail("external_link_not_relative", str(link["index"]), "portable relative external source target", link["uri"])
    verify_layout_and_styles(inventory, oracle, failures)
    comparison = None
    if reference is not None:
        comparison = compare_published(inventory, read_xlsx(reference), oracle)
        for difference in comparison["unexplainedDifferences"]:
            failures.append({"code": "unexplained_published_difference", **difference})
    return {"passed": not failures, "failures": failures, "inventory": inventory, "publishedComparison": comparison}


def style_difference_paths(before, after, prefix=""):
    if isinstance(before, dict) and isinstance(after, dict):
        result = []
        for key in sorted(set(before) | set(after)):
            result.extend(style_difference_paths(before.get(key), after.get(key), prefix + "." + key if prefix else key))
        return result
    return [] if before == after else [prefix]


def style_override_allowed(before, after, source_fields):
    approved = {"fontFamily": ["font.name"], "fontSize": ["font.size"], "bold": ["font.bold"], "italic": ["font.italic"], "color": ["font.color"], "fill": ["fill.pattern", "fill.color"], "numberFormat": ["numberFormat"], "alignment": ["alignment.horizontal"], "indent": ["alignment.indent"], "border": ["border"]}
    allowed = [prefix for field in source_fields for prefix in approved.get(field, [])]
    paths = style_difference_paths(before, after)
    return bool(paths) and all(any(path == prefix or path.startswith(prefix + ".") for prefix in allowed) for path in paths)


def column_style_indices(sheet, max_columns=16384):
    # A bounded 16K column vector avoids materializing a worksheet's cells.
    indices = [sheet.get("defaultStyleIndex", "0")] * max_columns
    for column in sheet["columns"]:
        if "style" in column:
            first, end = max(1, int(column["min"])) - 1, min(max_columns, int(column["max"]))
            indices[first:end] = [column["style"]] * max(0, end - first)
    return indices


def compare_unstored_styles(actual, reference, source_styles, max_columns=16384, max_rows=1048576):
    """Compare every blank default region through sparse row/column partitions."""
    failures, compared = [], 0
    actual_sheets = {sheet["name"]: sheet for sheet in actual["sheets"]}
    occupied = set(actual["cells"]) | set(reference["cells"])
    for before in reference["sheets"]:
        name = before["name"]
        if name not in actual_sheets:
            continue
        after = actual_sheets[name]
        old_columns, new_columns = column_style_indices(before, max_columns), column_style_indices(after, max_columns)
        columns = [0] + [index for index in range(1, max_columns) if old_columns[index] != old_columns[index - 1] or new_columns[index] != new_columns[index - 1]] + [max_columns]
        row_styles = lambda sheet: {int(row["r"]): row["s"] for row in sheet["rows"] if "s" in row and row.get("customFormat", "0") in ("1", "true")}
        old_rows, new_rows = row_styles(before), row_styles(after)
        rows = sorted({1, max_rows + 1} | {boundary for row in set(old_rows) | set(new_rows) for boundary in (row, row + 1)})
        for first_row, end_row in zip(rows, rows[1:]):
            for first_column, end_column in zip(columns, columns[1:]):
                old_style = reference["styleDefinitions"][old_rows.get(first_row, old_columns[first_column])]
                new_style = actual["styleDefinitions"][new_rows.get(first_row, new_columns[first_column])]
                compared += 1
                if old_style == new_style:
                    continue
                # Explicit cells are compared separately. A finite source style
                # override only excuses its documented fields and coordinates.
                mismatch = None
                for row in range(first_row, end_row):
                    for column in range(first_column + 1, end_column + 1):
                        address = name + "!" + get_column_letter(column) + str(row)
                        if address not in occupied and not style_override_allowed(old_style, new_style, source_styles.get(address, {})):
                            mismatch = address
                            break
                    if mismatch is not None:
                        break
                if mismatch is not None:
                    failures.append({"address": mismatch, "category": "unstored_effective_style", "fields": style_difference_paths(old_style, new_style), "expected": old_style, "actual": new_style,
                                     "region": get_column_letter(first_column + 1) + str(first_row) + ":" + get_column_letter(end_column) + str(end_row - 1)})
    return failures, compared


def compare_published(actual, reference, oracle):
    explained, unexplained = [], []
    exact_cache_differences = []
    exact_format_differences = []
    source_expectations = {}
    for record in source_input_checks(oracle):
        source_expectations[actual["sheets"][0]["name"] + "!" + record["address"]] = record
    source_styles = {}
    for entry in oracle.get("styleRanges", []):
        fields = dict(entry.get("style", {}))
        fields.update({key: entry[key] for key in ("numberFormat", "color", "fill") if key in entry})
        for address in addresses(entry["ranges"]):
            source_styles.setdefault(actual["sheets"][0]["name"] + "!" + address, {}).update(fields)
    cells = sorted(set(actual["cells"]) | set(reference["cells"]))
    for address in cells:
        after, before = actual["cells"].get(address), reference["cells"].get(address)
        source = source_expectations.get(address, {})
        if after is None or before is None:
            present = after or before
            if present["input"] in (None, ""):
                explained.append({"address": address, "category": "style_only_storage", "source": "Explicit storage differs; effective blank-cell styles are still compared using native row/column/default formats."})
                blank = lambda inventory: {"input": None, "cachedValue": None, "formula": None, "style": unstored_cell_style(inventory, address)}
                after = after if after is not None else blank(actual)
                before = before if before is not None else blank(reference)
            elif after is not None and "input" in source and same_value(after["input"], source["input"]):
                explained.append({"address": address, "category": "source_observed_extra_cell", "actual": after["input"]})
                continue
            else:
                unexplained.append({"address": address, "category": "missing_or_extra_nonempty_cell", "expected": before["input"] if before else None, "actual": after["input"] if after else None})
                continue
        if not same_value(after["input"], before["input"]):
            exact_source = "input" in source and same_value(after["input"], source["input"])
            if exact_source:
                explained.append({"address": address, "category": "source_input_override", "published": before["input"], "actual": after["input"]})
            else:
                unexplained.append({"address": address, "category": "input_or_formula", "expected": before["input"], "actual": after["input"]})
        # A source-observed nonformula text literal is both its input and its
        # value. The published spelling/spacing must not overrule it a second
        # time through cachedValue. Formula results retain independent checks.
        source_literal = source.get("input")
        exact_source_literal = isinstance(source_literal, str) and not source_literal.startswith("=") and after["formula"] is None and same_value(after["input"], source_literal)
        expected_cache = source_literal if exact_source_literal else before["cachedValue"]
        if numeric(after["cachedValue"]) and numeric(expected_cache) and after["cachedValue"] != expected_cache:
            exact_cache_differences.append({"address": address, "published": expected_cache, "actual": after["cachedValue"], "absoluteDifference": abs(after["cachedValue"] - expected_cache), "withinTolerance": same_value(after["cachedValue"], expected_cache)})
        if address.endswith("110") and source.get("input", "").startswith("=("):
            name, cell_address = address.split("!", 1)
            column = re.match(r"[A-Z]+", cell_address).group()
            parts = [reference["cells"].get(name + "!" + column + str(row), {}).get("cachedValue") for row in [109,114,72]]
            if all(numeric(value) for value in parts) and parts[2] != 0:
                expected_cache = (parts[0] + parts[1]) / parts[2]
        blank_override = source.get("input", object()) == "" and after["input"] in (None, "")
        if not blank_override and not same_value(after["cachedValue"], expected_cache):
            unexplained.append({"address": address, "category": "formula_cache_or_literal_value", "expected": expected_cache, "actual": after["cachedValue"]})
        source_fields = source_styles.get(address, {})
        actual_code, published_code = after["style"]["numberFormat"], before["style"]["numberFormat"]
        # income079's retained Custom Type field is exactly 0.0%;(0.0%).
        # Admit only its published escaped-parentheses counterpart; preserve
        # both raw codes and continue comparing every other style field.
        known_percent_equivalent = (actual_code, published_code) == ("0.0%;(0.0%)", r"0.0%;\(0.0%\)") and source_fields.get("numberFormat", actual_code) == actual_code
        if known_percent_equivalent:
            source_fields = {**source_fields, "numberFormat": actual_code}
        if actual_code != published_code:
            observed_code = source_fields.get("numberFormat") == actual_code
            # These exact pairs remain raw-code differences, never exact-style
            # matches or a general normalization of format escape characters.
            known_fy_equivalent = observed_code and actual_code == '"FY" yy' and published_code == '"FY"\\ yy'
            exact_format_differences.append({"address": address, "published": published_code, "actual": actual_code, "classification": "source_observed_raw_code" if observed_code else "unexplained_raw_code", "semanticEquivalent": known_fy_equivalent or known_percent_equivalent})
        if after["style"] != before["style"]:
            paths = style_difference_paths(before["style"], after["style"])
            if style_override_allowed(before["style"], after["style"], source_fields):
                explained.append({"address": address, "category": "source_style_override", "fields": paths, "published": before["style"], "actual": after["style"]})
            else:
                unexplained.append({"address": address, "category": "effective_style", "fields": paths, "expected": before["style"], "actual": after["style"]})
    blank_failures, blank_regions = compare_unstored_styles(actual, reference, source_styles)
    unexplained.extend(blank_failures)
    return {"referencePath": reference["path"], "referenceSha256": reference["sha256"], "storedCellUnionCompared": len(cells), "unstoredStyleRegionsCompared": blank_regions, "publishedStoredCells": reference["storedCells"], "actualStoredCells": actual["storedCells"], "exactNumericCacheDifferences": exact_cache_differences, "exactNumberFormatCodeDifferences": exact_format_differences, "explainedDifferences": explained, "unexplainedDifferences": unexplained}


def addresses(ranges):
    for area in ranges:
        left, top, right, bottom = range_boundaries(area)
        if (right - left + 1) * (bottom - top + 1) > 100000:
            raise ValueError("An audit range must be finite and bounded")
        for row in range(top, bottom + 1):
            for column in range(left, right + 1):
                yield get_column_letter(column) + str(row)


def native_style(style):
    return {"fontFamily": style["font"]["name"], "fontSize": style["font"]["size"], "bold": style["font"]["bold"], "italic": style["font"]["italic"], "color": style["font"]["color"], "fill": style["fill"]["color"] if style["fill"]["pattern"] else "none", "numberFormat": style["numberFormat"], "alignment": style["alignment"]["horizontal"], "indent": style["alignment"]["indent"], "border": style["border"]}


def verify_layout_and_styles(inventory, oracle, failures):
    sheet = inventory["sheets"][inventory["activeTab"]]
    name = sheet["name"]
    def fail(code, field, expected, actual):
        failures.append({"code": code, "address": field, "expected": expected, "actual": actual})
    defaults = oracle.get("sheetDefaults", {})
    if "defaultRowHeight" in defaults:
        actual = float(sheet["format"].get("defaultRowHeight", "15"))
        if actual != defaults["defaultRowHeight"]:
            fail("sheet_default", "defaultRowHeight", defaults["defaultRowHeight"], actual)
    font_fields = {"fontFamily": "name", "fontSize": "size"}
    def check_default_font(style, address):
        for expected_key, font_key in font_fields.items():
            if expected_key in defaults and style["font"][font_key] != defaults[expected_key]:
                fail("sheet_default", address + "." + expected_key, defaults[expected_key], style["font"][font_key])
    if any(field in defaults for field in font_fields):
        # Compare sparse column partitions rather than materializing the grid.
        boundaries = {1, 16385}
        for column in sheet["columns"]:
            boundaries.update((max(1, int(column["min"])), min(16385, int(column["max"]) + 1)))
        boundaries = sorted(boundary for boundary in boundaries if 1 <= boundary <= 16385)
        for first, end in zip(boundaries, boundaries[1:]):
            column = next((column for column in reversed(sheet["columns"]) if int(column["min"]) <= first <= int(column["max"]) and "style" in column), None)
            style = inventory["styleDefinitions"][column["style"] if column else "0"]
            check_default_font(style, "columns " + get_column_letter(first) + ":" + get_column_letter(end - 1))
        for row in sheet["rows"]:
            if "s" in row and row.get("customFormat", "0") in ("1", "true"):
                check_default_font(inventory["styleDefinitions"][row["s"]], "row " + row["r"])
    expected_view = oracle.get("view", {})
    view = sheet["views"][0] if sheet["views"] else {"attributes": {}, "selections": []}
    attributes = view["attributes"]
    top_left = attributes.get("topLeftCell", "A1")
    column, row = coordinate_from_string(top_left)
    selection = view["selections"][0] if view["selections"] else {}
    actual_view = {"sheetName": name, "activeCell": selection.get("activeCell", "A1"), "topRow": row, "firstColumn": column, "showGridLines": attributes.get("showGridLines", "1") not in ("0", "false"), "zoomPercent": int(attributes.get("zoomScale", "100"))}
    for key, value in actual_view.items():
        if key in expected_view and expected_view[key] != value:
            fail("saved_view", key, expected_view[key], value)
    if "activeCell" in expected_view:
        expected_ranges = expected_view.get("selectedRanges", [expected_view["activeCell"]])
        actual_ranges = [area for item in view["selections"] for area in item.get("sqref", "A1").split()]
        if len(view["selections"]) != 1 or [range_boundaries(area) for area in actual_ranges] != [range_boundaries(area) for area in expected_ranges]:
            fail("saved_view", "selectedRanges", expected_ranges, actual_ranges)
    if "mergedRanges" in oracle and sorted(oracle["mergedRanges"]) != sorted(sheet["merges"]):
        fail("merged_ranges", name, oracle["mergedRanges"], sheet["merges"])
    for column_name, expected in oracle.get("columnsPixels", {}).items():
        index = column_index_from_string(column_name)
        dimension = next((item for item in reversed(sheet["columns"]) if int(item["min"]) <= index <= int(item["max"]) and "width" in item), None)
        actual = math.floor(((256 * float(dimension["width"]) + 18) / 256) * 7) if dimension else math.floor(float(sheet["format"].get("defaultColWidth", 8.43)) * 7 + 5)
        if actual != expected:
            fail("column_pixels_mdw7", column_name, expected, actual)
    for expected in oracle.get("styleRanges", []):
        wanted = dict(expected.get("style", {}))
        wanted.update({key: expected[key] for key in ("numberFormat", "color", "fill") if key in expected})
        for address in addresses(expected["ranges"]):
            cell = inventory["cells"].get(name + "!" + address)
            if cell is None:
                fail("missing_style_cell", address, wanted, None)
                continue
            style = native_style(cell["style"])
            for key, value in wanted.items():
                actual = style.get(key)
                equal = actual.casefold() == value.casefold() if isinstance(actual, str) and isinstance(value, str) and key in ("color", "fill") else actual == value
                if not equal:
                    fail("source_style", address + "." + key, value, actual)
    for expected in oracle.get("calculationChecks", []):
        for address in addresses([expected["range"]]):
            value = inventory["cells"].get(name + "!" + address, {}).get("cachedValue")
            if "value" in expected and not same_value(value, expected["value"]):
                fail("calculation_value", address, expected["value"], value)
            if expected.get("exactZero") and (not numeric(value) or value != 0):
                fail("calculation_exact_zero", address, 0, value)
            if "absoluteBound" in expected and (not numeric(value) or abs(value) >= expected["absoluteBound"]):
                fail("calculation_bound", address, expected["absoluteBound"], value)


def biff_style(book, sheet, row, column, xf_index=None):
    xf = book.xf_list[sheet.cell_xf_index(row, column) if xf_index is None else xf_index]
    font = book.font_list[xf.font_index]
    def color(index, default=(0,0,0)):
        return book.colour_map.get(index) or default
    border = xf.border
    return {"font": {"name": font.name, "heightTwips": font.height, "bold": bool(font.bold or font.weight >= 700), "italic": bool(font.italic), "underline": font.underline_type, "color": color(font.colour_index)}, "numberFormat": book.format_map[xf.format_key].format_str, "fill": {"pattern": xf.background.fill_pattern, "color": color(xf.background.pattern_colour_index) if xf.background.fill_pattern else None}, "border": {side: {"style": getattr(border, side + "_line_style"), "color": color(getattr(border, side + "_colour_index"))} for side in ("top", "bottom", "left", "right") if getattr(border, side + "_line_style")}, "alignment": {key: getattr(xf.alignment, key) for key in ("hor_align", "vert_align", "indent_level", "text_wrapped", "rotation", "shrink_to_fit")}}


def biff_views_from_stream(stream):
    """Independent native WINDOW1/WINDOW2/SELECTION/SCL inspection."""
    def records(offset):
        while offset + 4 <= len(stream):
            kind, size = struct.unpack_from("<HH", stream, offset)
            end = offset + 4 + size
            if end > len(stream):
                raise ValueError("Truncated BIFF record in view inventory")
            yield kind, stream[offset + 4:end]
            if kind == 0x000A:
                return
            offset = end
        raise ValueError("Missing BIFF end record in view inventory")
    active, offsets = 0, []
    for kind, data in records(0):
        if kind == 0x003D:
            active = struct.unpack_from("<H", data, 10)[0]
        elif kind == 0x0085 and len(data) >= 6 and data[5] == 0:
            offsets.append(struct.unpack_from("<I", data)[0])
    sheets = []
    for offset in offsets:
        view = {"activeCell": "A1", "selectedRanges": ["A1"], "topRow": 1, "firstColumn": "A", "zoomPercent": 100,
                "showGridLines": True, "showHeadings": True, "selected": False, "displayed": False}
        for kind, data in records(offset):
            if kind == 0x023E:
                flags, row, column = struct.unpack_from("<3H", data)
                view.update(topRow=row + 1, firstColumn=get_column_letter(column + 1), showGridLines=bool(flags & 2), showHeadings=bool(flags & 4), selected=bool(flags & 0x200), displayed=bool(flags & 0x400))
                if len(data) >= 14:
                    view["zoomPercent"] = struct.unpack_from("<H", data, 12)[0] or 100
            elif kind == 0x001D:
                pane, row, column, active_range, count = struct.unpack_from("<B4H", data)
                if len(data) != 9 + count * 6 or count == 0:
                    raise ValueError("Invalid BIFF selection record")
                if pane == 3:
                    areas = []
                    for index in range(count):
                        top, bottom, left, right = struct.unpack_from("<2H2B", data, 9 + index * 6)
                        first, last = get_column_letter(left + 1) + str(top + 1), get_column_letter(right + 1) + str(bottom + 1)
                        areas.append(first if first == last else first + ":" + last)
                    view.update(activeCell=get_column_letter(column + 1) + str(row + 1), selectedRanges=areas, activeRangeIndex=active_range)
            elif kind == 0x00A0:
                numerator, denominator = struct.unpack("<2H", data)
                if denominator == 0:
                    raise ValueError("Invalid BIFF zoom denominator")
                view["zoomPercent"] = numerator * 100 / denominator
        sheets.append(view)
    return {"activeSheetIndex": active, "sheets": sheets}


def read_xls(path):
    import xlrd
    import xlrd.compdoc
    path = Path(path)
    book = xlrd.open_workbook(path, formatting_info=True)
    compound = xlrd.compdoc.CompDoc(path.read_bytes())
    stream = compound.get_named_stream("Workbook") or compound.get_named_stream("Book")
    if stream is None:
        raise ValueError("Missing native BIFF workbook stream")
    views = biff_views_from_stream(stream)
    style_definitions = {str(index): biff_style(book, None, 0, 0, index) for index in range(len(book.xf_list))}
    sheets = []
    for sheet in book.sheets():
        cells = {}
        for row in range(sheet.nrows):
            for column in range(sheet.row_len(row)):
                value = sheet.cell(row, column)
                if value.ctype == xlrd.XL_CELL_EMPTY:
                    continue
                cells[get_column_letter(column + 1) + str(row + 1)] = {"row": row + 1, "column": column + 1, "type": value.ctype, "value": value.value, "style": biff_style(book, sheet, row, column)}
        layout = {"defaultRowHeightTwips": sheet.default_row_height,
                  "rowHeights": {str(row + 1): info.height for row, info in sheet.rowinfo_map.items() if not info.has_default_height and info.height != sheet.default_row_height},
                  "hiddenRows": sorted(row + 1 for row, info in sheet.rowinfo_map.items() if info.hidden),
                  "hiddenColumns": sorted(column + 1 for column, info in sheet.colinfo_map.items() if info.hidden or info.width == 0),
                  "columnWidths": {get_column_letter(column + 1): sheet.computed_column_width(column) for column in range(256)}}
        sheets.append({"name": sheet.name, "rows": sheet.nrows, "columns": sheet.ncols, "cells": cells, "merges": [list(value) for value in sheet.merged_cells], "nativeView": views["sheets"][sheet.number], "layout": layout,
                       "defaultStyleIndex": "15" if len(book.xf_list) > 15 else "0",
                       "rowStyleIndices": {str(row + 1): str(info.xf_index) for row, info in sheet.rowinfo_map.items() if info.xf_index >= 0},
                       "columnStyleIndices": {str(column + 1): str(info.xf_index if info.xf_index >= 0 else 15) for column, info in sheet.colinfo_map.items()}})
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "sheetCount": len(sheets), "storedCells": sum(len(sheet["cells"]) for sheet in sheets), "sheets": sheets, "styleDefinitions": style_definitions, "activeSheetIndex": views["activeSheetIndex"]}


def biff_unstored_cell_style(inventory, sheet, address):
    column, row = coordinate_from_string(address)
    index = sheet["rowStyleIndices"].get(str(row), sheet["columnStyleIndices"].get(str(column_index_from_string(column)), sheet["defaultStyleIndex"]))
    return inventory["styleDefinitions"][index]


def biff_blank_inventory(inventory, names):
    sheets, cells = [], {}
    for sheet in inventory["sheets"]:
        if sheet["name"] not in names:
            continue
        cells.update({sheet["name"] + "!" + address: cell for address, cell in sheet["cells"].items()})
        sheets.append({"name": sheet["name"], "defaultStyleIndex": sheet["defaultStyleIndex"],
                       "columns": [{"min": column, "max": column, "style": index} for column, index in sheet["columnStyleIndices"].items()],
                       "rows": [{"r": row, "s": index, "customFormat": "1"} for row, index in sheet["rowStyleIndices"].items()]})
    return {"cells": cells, "sheets": sheets, "styleDefinitions": inventory["styleDefinitions"]}


def verify_biff_source_view(inventory, expected, failures):
    if not expected:
        return
    index = inventory.get("activeSheetIndex", -1)
    if not 0 <= index < len(inventory["sheets"]):
        failures.append({"code": "source_saved_view", "field": "sheetIndex", "expected": expected.get("sheetIndex"), "actual": index})
        return
    sheet = inventory["sheets"][index]
    actual = {"sheetIndex": index, "sheetName": sheet["name"], **sheet.get("nativeView", {})}
    for key, value in expected.items():
        if actual.get(key) != value:
            failures.append({"code": "source_saved_view", "field": key, "expected": value, "actual": actual.get(key)})
    for key in ("selected", "displayed"):
        indices = [position for position, item in enumerate(inventory["sheets"]) if item.get("nativeView", {}).get(key)]
        if indices != [expected["sheetIndex"]]:
            failures.append({"code": "source_saved_view", "field": key + "SheetIndices", "expected": [expected["sheetIndex"]], "actual": indices})


def verify_biff_primary_layout(sheet, original, expected, failures):
    if not expected:
        return
    actual = sheet.get("layout", {})
    def check(field, value, got):
        if value != got:
            failures.append({"code": "edited_source_layout", "sheet": sheet["name"], "field": field, "expected": value, "actual": got})
    for key in ("defaultRowHeightTwips", "hiddenRows", "hiddenColumns"):
        if key in expected:
            check(key, expected[key], actual.get(key))
    if "rowHeightTwips" in expected:
        for row, height in actual.get("rowHeights", {}).items():
            check("row" + row + ".heightTwips", expected["rowHeightTwips"], height)
    for column, width in expected.get("columnWidths", {}).items():
        check("column" + column + ".width256", width, actual.get("columnWidths", {}).get(column))
    for target, source in expected.get("preservedColumnWidths", {}).items():
        check("column" + target + ".width256", original["layout"]["columnWidths"][source], actual.get("columnWidths", {}).get(target))


def audit_source_xls(source, raw_reference, source_oracle=None):
    source_oracle = source_oracle or {}
    actual, reference = read_xls(source), read_xls(raw_reference)
    failures, intended_changes = [], []
    if [sheet["name"] for sheet in actual["sheets"]] != [sheet["name"] for sheet in reference["sheets"]]:
        failures.append({"code": "source_sheet_order", "expected": [sheet["name"] for sheet in reference["sheets"]], "actual": [sheet["name"] for sheet in actual["sheets"]]})
    primary = {"operations", "consolidated balance sheets", "cash flows"}
    zeroes = {"C37","D37","C38","D38","C39","C40","C41","E41","C43","C44","D44","E46","C30","E12"}
    matched_values = matched_styles = matched_primary_styles = 0
    by_name = {sheet["name"]: sheet for sheet in actual["sheets"]}
    for old in reference["sheets"]:
        name = old["name"]
        if name not in by_name:
            continue
        new = by_name[name]
        if name not in primary:
            for address in sorted(set(old["cells"]) | set(new["cells"])):
                before, after = old["cells"].get(address), new["cells"].get(address)
                nonempty_before = before is not None and before["type"] != 6
                nonempty_after = after is not None and after["type"] != 6
                if nonempty_before != nonempty_after or (nonempty_before and (before["type"] != after["type"] or not same_value(before["value"], after["value"]))):
                    failures.append({"code": "unmodified_source_value", "sheet": name, "address": address, "expected": before, "actual": after})
                else:
                    matched_values += bool(nonempty_before)
                before_style = before["style"] if before is not None else biff_unstored_cell_style(reference, old, address)
                after_style = after["style"] if after is not None else biff_unstored_cell_style(actual, new, address)
                if before_style != after_style:
                    failures.append({"code": "unmodified_source_style", "sheet": name, "address": address, "expected": before_style, "actual": after_style})
                else:
                    matched_styles += 1
            if old["merges"] != new["merges"]:
                failures.append({"code": "unmodified_source_merges", "sheet": name, "expected": old["merges"], "actual": new["merges"]})
            continue
        expected = {}
        for address, cell in old["cells"].items():
            if cell["type"] == 6:
                continue
            row, column = cell["row"], cell["column"]
            if name == "operations":
                column = {7:9,13:8,19:7}.get(column, column)
            elif name == "consolidated balance sheets":
                column = {7:3,13:2}.get(column, column)
            else:
                column = {7:5,13:4,19:3}.get(column, column)
                if row >= 54:
                    row -= 4
            target = get_column_letter(column) + str(row)
            wanted = {"type": cell["type"], "value": cell["value"], "style": cell["style"]}
            if name == "cash flows" and target in zeroes:
                wanted.update(type=2, value=0)
            expected[target] = wanted
        for address in sorted(set(expected) | {address for address, cell in new["cells"].items() if cell["type"] != 6}):
            before, after = expected.get(address), new["cells"].get(address)
            if before is None or after is None or before["type"] != after["type"] or not same_value(before["value"], after["value"]):
                failures.append({"code": "edited_source_value", "sheet": name, "address": address, "expected": before, "actual": after})
            else:
                matched_values += 1
            if source_oracle.get("preserveProjectedPrimaryStyles") and before is not None and after is not None:
                if before["style"] != after["style"]:
                    failures.append({"code": "edited_source_style", "sheet": name, "address": address, "expected": before["style"], "actual": after["style"]})
                else:
                    matched_primary_styles += 1
        if name in source_oracle.get("primaryMerges", {}) and sorted(new["merges"]) != sorted(source_oracle["primaryMerges"][name]):
            failures.append({"code": "edited_source_merges", "sheet": name, "expected": source_oracle["primaryMerges"][name], "actual": new["merges"]})
        verify_biff_primary_layout(new, old, source_oracle.get("primaryLayout", {}).get(name), failures)
        intended_changes.append({"sheet": name, "expectedNonemptyCells": len(expected), "actualNonemptyCells": sum(cell["type"] != 6 for cell in new["cells"].values()), "method": "Independent original BIFF values projected through source-observed year-column moves, cashflow blankrow50:53 deletion, and explicit emdash-to-zero edits. Primary-sheet formatting changes are inventoried rather than mistaken for untouched-style requirements."})
    verify_biff_source_view(actual, source_oracle.get("view"), failures)
    untouched = {sheet["name"] for sheet in reference["sheets"]} - primary
    blank_failures, blank_regions = compare_unstored_styles(biff_blank_inventory(actual, untouched), biff_blank_inventory(reference, untouched), {}, max_columns=256, max_rows=65536)
    failures.extend({"code": "unmodified_source_blank_style", **failure} for failure in blank_failures)
    return {"passed": not failures, "failures": failures, "actualSheetCount": actual["sheetCount"], "originalSheetCount": reference["sheetCount"], "matchedNonemptyValues": matched_values, "matchedUnmodifiedStyles": matched_styles, "matchedPrimaryStyles": matched_primary_styles, "unstoredUnmodifiedStyleRegionsCompared": blank_regions, "intendedPrimaryChanges": intended_changes, "inventory": actual, "referenceSha256": reference["sha256"]}


def json_safe(value):
    if isinstance(value, float) and not math.isfinite(value):
        return str(value)
    if isinstance(value, dict):
        return {key: json_safe(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [json_safe(item) for item in value]
    return value


def audit_external_caches(model, source):
    failures, matched = [], 0
    names = [sheet["name"] for sheet in source["sheets"]]
    expected = {}
    for sheet in source["sheets"]:
        for address, cell in sheet["cells"].items():
            if cell["type"] == 6:
                continue
            value = bool(cell["value"]) if cell["type"] == 4 else cell["value"]
            if cell["type"] == 5:
                import xlrd
                value = xlrd.error_text_from_code.get(value, "#UNKNOWN!")
            expected[sheet["name"] + "!" + address] = value
    for link in model["externalLinks"]:
        if link.get("sheetNames") != names:
            failures.append({"code": "external_cache_sheet_names", "index": link["index"], "expected": names, "actual": link.get("sheetNames", [])})
        cached = link.get("cachedCells", {})
        for address in sorted(set(expected) | set(cached)):
            value, actual = expected.get(address), cached.get(address)
            if not same_value(actual, value):
                failures.append({"code": "external_source_cache", "index": link["index"], "address": address, "expected": value, "actual": actual})
            elif address in expected:
                matched += 1
    return {"passed": not failures, "failures": failures, "matchedCachedValues": matched}


def main(argv=None):
    base = ROOT / "docs/validation/otis-90-minute"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workbook", type=Path)
    parser.add_argument("--source", type=Path, required=True, help="The actual retained edited raw.xls paired with this native model")
    parser.add_argument("--reference", type=Path, default=base / "reference/101-01-90-Minute-Case-Study-Otis-Complete.xlsx")
    parser.add_argument("--raw-reference", type=Path, default=base / "reference/101-01-Otis-10-K-Raw.xls")
    parser.add_argument("--oracle", type=Path, default=base / "final-native-acceptance-oracle.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        oracle = json.loads(args.oracle.read_text())
        model = audit_xlsx(args.workbook, args.reference, oracle)
        source = audit_source_xls(args.source, args.raw_reference, source_oracle=oracle.get("retainedSource"))
        external_caches = audit_external_caches(model["inventory"], source["inventory"])
        bindings = []
        for link in model["inventory"]["externalLinks"]:
            if not link["relative"] or Path(link["resolvedPath"]).resolve() != args.source.resolve():
                bindings.append({"code": "external_source_binding", "index": link["index"], "expected": str(args.source.resolve()), "actual": link["resolvedPath"]})
        report = {"passed": model["passed"] and source["passed"] and external_caches["passed"] and not bindings, "sourceReviewComplete": bool(oracle.get("sourceReviewComplete", False)), "method": "Independent read-only ZIP/XML/openpyxl inspection of every stored native model cell and cache; xlrd inspection of all120 retained source sheets; source-video oracle takes precedence over published-reference differences.", "model": model, "retainedSource": source, "externalSourceCaches": external_caches, "linkBindingFailures": bindings, "limits": ["Passing these implemented file checks does not certify every video gesture or pixel identity.", "Ribbon collapse, intra-row pixel offsets, popups and keyboard behavior require the separately retained native execution trace.", "Unexplained published-reference differences remain failures rather than being silently accepted."]}
    except Exception as error:
        report = {"passed": False, "sourceReviewComplete": False, "error": type(error).__name__ + ": " + str(error)}
        status = 2
    else:
        status = 0 if report["passed"] else 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(json_safe(report), indent=2, allow_nan=False) + "\n")
    print(json.dumps({"passed": report["passed"], "report": str(args.output), "modelFailures": len(report.get("model", {}).get("failures", [])), "sourceFailures": len(report.get("retainedSource", {}).get("failures", [])), "error": report.get("error")}))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
