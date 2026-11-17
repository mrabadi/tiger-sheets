#!/usr/bin/env python3
"""Read-only audit of the native Otis replica against the official workbook.

The eight final row-110 ratios are the independently observed video override.
This checks workbook semantics and effective styles; it does not replay gestures
or establish pixel identity. No workbook is modified.
"""

import argparse
import colorsys
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZipFile

import openpyxl
from openpyxl.styles.colors import COLOR_INDEX


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REFERENCE = ROOT / "docs/validation/otis-90-minute/reference/101-01-90-Minute-Case-Study-Otis-Complete.xlsx"
DEFAULT_REPLICA = ROOT / "output/otis-90-minute/otis-three-statement-replica.xlsx"
DEFAULT_OUTPUT = ROOT / "docs/validation/otis-90-minute/replica-independent-audit.json"
NS = {"a": "http://schemas.openxmlformats.org/drawingml/2006/main"}


def theme_palette(book):
    scheme = ET.fromstring(book.loaded_theme).find("a:themeElements/a:clrScheme", NS) if book.loaded_theme else None
    palette = {}
    if scheme is not None:
        for entry in scheme:
            value = next(iter(entry))
            palette[entry.tag.rsplit("}", 1)[-1]] = value.get("lastClr", value.get("val"))
    order = ["lt1", "dk1", "lt2", "dk2"] + [f"accent{i}" for i in range(1, 7)] + ["hlink", "folHlink"]
    return [palette.get(name) for name in order]


def color_value(color, palette, default=None):
    if color is None:
        return default
    if color.type == "rgb":
        rgb = color.rgb[-6:]
    elif color.type == "theme":
        rgb = palette[color.theme]
    elif color.type == "indexed":
        rgb = COLOR_INDEX[color.indexed][-6:] if color.indexed < len(COLOR_INDEX) else ("FFFFFF" if color.indexed == 65 else "000000")
    else:
        return default
    if rgb is None:
        return default
    channels = tuple(int(rgb[i:i + 2], 16) / 255 for i in (0, 2, 4))
    if color.tint:
        hue, lightness, saturation = colorsys.rgb_to_hls(*channels)
        lightness = lightness * (1 + color.tint) if color.tint < 0 else lightness * (1 - color.tint) + color.tint
        channels = colorsys.hls_to_rgb(hue, lightness, saturation)
    return "#" + "".join(f"{round(channel * 255):02X}" for channel in channels)


def font_value(font, palette):
    return {"name": font.name, "size": font.sz, "bold": bool(font.b), "italic": bool(font.i), "underline": font.u or False, "color": color_value(font.color, palette, "#000000")}


def style_value(cell, palette):
    fill = cell.fill
    borders = {}
    for side in ("top", "bottom", "left", "right"):
        border = getattr(cell.border, side)
        if border is not None and border.style:
            borders[side] = {"style": border.style, "color": color_value(border.color, palette, "#000000")}
    alignment = cell.alignment
    return {"font": font_value(cell.font, palette), "fill": {"pattern": fill.patternType, "color": color_value(fill.fgColor, palette) if fill.patternType else None}, "border": borders, "alignment": {"horizontal": alignment.horizontal or "general", "vertical": alignment.vertical or "bottom", "indent": alignment.indent or 0, "wrap": bool(alignment.wrap_text)}, "numberFormat": cell.number_format}


def column_layout(sheet):
    result = [(sheet.sheet_format.defaultColWidth, False)] * 16384
    for dimension in sheet.column_dimensions.values():
        start, end = dimension.min, dimension.max
        for index in range(start - 1, end):
            result[index] = (dimension.width, bool(dimension.hidden))
    return result


def row_layout(sheet, rows):
    return [(row, sheet.row_dimensions[row].height or sheet.sheet_format.defaultRowHeight, bool(sheet.row_dimensions[row].hidden)) for row in rows]


def whole_sheet_fonts(sheet, palette):
    values = set()
    for dimension in sheet.column_dimensions.values():
        values.add(json.dumps(font_value(dimension.font, palette), sort_keys=True))
    return [json.loads(value) for value in sorted(values)]


def saved_selection(path):
    with ZipFile(path) as archive:
        sheet = ET.fromstring(archive.read("xl/worksheets/sheet1.xml"))
    return [dict(selection.attrib) for selection in sheet.findall("{*}sheetViews/{*}sheetView/{*}selection")]


def numeric_equal(actual, expected):
    return isinstance(actual, (int, float)) and not isinstance(actual, bool) and math.isfinite(actual) and math.isclose(actual, expected, abs_tol=1e-8, rel_tol=1e-10)


def audit(replica_path, reference_path):
    original = openpyxl.load_workbook(reference_path, data_only=False)
    source_values = openpyxl.load_workbook(reference_path, data_only=True)
    replica = openpyxl.load_workbook(replica_path, data_only=False)
    replica_values = openpyxl.load_workbook(replica_path, data_only=True)
    source, actual = original.worksheets[0], replica.worksheets[0]
    expected_values, cached = source_values.worksheets[0], replica_values.worksheets[0]
    source_palette, actual_palette = theme_palette(original), theme_palette(replica)
    expected_cells, actual_cells = source._cells, actual._cells
    input_styles, caches, exact_caches, zero_sections, layout, row110 = [], [], [], [], [], []
    addresses = sorted(set(expected_cells) | set(actual_cells))
    formula_count = cached_count = nonempty_count = 0
    for row, column in addresses:
        before, after = source.cell(row, column), actual.cell(row, column)
        override = row == 110 and 3 <= column <= 10
        expected_input = f"=({before.column_letter}109+{before.column_letter}114)/{before.column_letter}72" if override else before.value
        expected_style = style_value(before, source_palette)
        if override:
            expected_style["font"]["bold"] = False
            expected_style["font"]["color"] = "#000000"
            expected_style["numberFormat"] = "0%"
        found_style = style_value(after, actual_palette)
        if expected_input != after.value or expected_style != found_style:
            input_styles.append({"address": after.coordinate, "expected_input": expected_input, "actual_input": after.value, "expected_style": expected_style, "actual_style": found_style})
        nonempty_count += after.value is not None
        if after.data_type != "f":
            continue
        formula_count += 1
        actual_value = cached.cell(row, column).value
        cached_count += actual_value is not None
        expected_value = ((expected_values.cell(109, column).value + expected_values.cell(114, column).value) / expected_values.cell(72, column).value) if override else expected_values.cell(row, column).value
        equal = numeric_equal(actual_value, expected_value) if isinstance(expected_value, (int, float)) and not isinstance(expected_value, bool) else actual_value == expected_value
        if actual_value != expected_value:
            exact_caches.append({"address": after.coordinate, "expected": expected_value, "actual": actual_value})
            if expected_value == 0 and isinstance(actual_value, (int, float)) and actual_value != 0:
                zero_sections.append({"address": after.coordinate, "expected": expected_value, "actual": actual_value, "expected_format_section": "zero", "actual_format_section": "negative" if actual_value < 0 else "positive", "number_format": after.number_format})
        if not equal:
            caches.append({"address": after.coordinate, "expected": expected_value, "actual": actual_value})
        if override:
            row110.append({"address": after.coordinate, "formula": after.value, "cached_value": actual_value, "displayed_percent": round(actual_value * 100) if isinstance(actual_value, (int, float)) else None, "expected_from_original_excel_caches": expected_value, "format": after.number_format, "bold_effective": bool(after.font.b)})
    rows = sorted(set(source.row_dimensions) | set(actual.row_dimensions) | set(range(1, max(source.max_row, actual.max_row) + 1)))
    for field, expected, found in (("sheet_count", len(original.worksheets), len(replica.worksheets)), ("sheet_name", source.title, actual.title), ("column_widths_and_hidden", column_layout(source), column_layout(actual)), ("row_heights_and_hidden", row_layout(source, rows), row_layout(actual, rows)), ("defaultRowHeight", source.sheet_format.defaultRowHeight, actual.sheet_format.defaultRowHeight), ("showGridLines", source.sheet_view.showGridLines, actual.sheet_view.showGridLines), ("mergedRanges", sorted(str(value) for value in source.merged_cells.ranges), sorted(str(value) for value in actual.merged_cells.ranges)), ("freezePanes", source.freeze_panes, actual.freeze_panes)):
        if expected != found:
            layout.append({"field": field, "expected": expected, "actual": found})
    source_fonts, actual_fonts = whole_sheet_fonts(source, source_palette), whole_sheet_fonts(actual, actual_palette)
    if source_fonts != actual_fonts:
        layout.append({"field": "whole_sheet_column_fonts", "expected": source_fonts, "actual": actual_fonts})
    extra_limits = ["Nonzero native residuals versus exact-zero reference caches select different accounting format sections; see zero_format_section_differences. This is a real display mismatch despite tolerance acceptance."] if zero_sections else []
    return {"replica_file": str(replica_path.resolve()), "reference_file": str(reference_path.resolve()), "sha256": hashlib.sha256(replica_path.read_bytes()).hexdigest(), "method": "Read-only openpyxl inspection; effective native styles resolve theme/tint and omitted boolean flags. Every final formula cache is independently compared against original Excel caches, with row110 ratios independently derived from original source caches. Tolerance acceptance is separate from exact numeric identity and sign-based format-section selection.", "compared_stored_cells": len(expected_cells), "replica_stored_cells": len(actual_cells), "replica_nonempty_cells": nonempty_count, "replica_formula_cells": formula_count, "input_and_effective_style_differences": input_styles, "layout_differences": layout, "formula_cache_differences": caches, "formula_cache_tolerance": {"absolute": 1e-8, "relative": 1e-10}, "formula_caches_exactly_equal": not exact_caches, "exact_formula_cache_differences": exact_caches, "zero_format_section_differences": zero_sections, "row110": row110, "bold_false_explanation": "OOXML omission of <b> is effective false; native JSON key absence is semantically equivalent to false.", "cached_formula_values_available": cached_count, "whole_sheet_column_fonts": {"source": source_fonts, "replica": actual_fonts, "match": source_fonts == actual_fonts}, "saved_selection": {"published_reference": saved_selection(reference_path), "replica": saved_selection(replica_path), "note": "Selection state is outside the input/style/dimension audit."}, "limits": extra_limits + ["SpreadsheetML metadata such as font family classification and scheme are not compared when explicit font face and visible style agree.", "Pixel identity, external source-link retention and exhaustive video gesture replay remain unverified.", "Final video selection was not established by this audit; saved selections are reported separately."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workbook", nargs="?", type=Path, default=DEFAULT_REPLICA)
    parser.add_argument("output_json", nargs="?", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--reference", type=Path, default=DEFAULT_REFERENCE)
    args = parser.parse_args()
    result = audit(args.workbook, args.reference)
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(result, indent=2, default=str) + "\n")
    passed = not any(result[key] for key in ("input_and_effective_style_differences", "layout_differences", "formula_cache_differences")) and result["replica_formula_cells"] == result["cached_formula_values_available"] == 520 and result["replica_stored_cells"] == 777 and result["replica_nonempty_cells"] == 752
    print(f"{'PASS within tolerance' if passed else 'FAIL'}: {result['replica_formula_cells']} formulas, {result['cached_formula_values_available']} caches; {len(result['exact_formula_cache_differences'])} exact numeric differences, {len(result['zero_format_section_differences'])} zero-section differences; SHA256 {result['sha256']}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
