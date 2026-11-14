#!/usr/bin/env python3
"""Independently inspect the tutorial XLSX and compare recorded UI text.

Uses openpyxl rather than TigerSheets' reader. Does not execute Excel.
"""
import argparse
import json
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path

import openpyxl
from openpyxl.utils import get_column_letter


def verify(directory: Path) -> dict:
    workbook = openpyxl.load_workbook(directory / "video-replicated.xlsx")
    assert workbook.sheetnames == ["Model", "Input"]
    model, inputs = workbook["Model"], workbook["Input"]
    assert model["C3"].value.isoformat().startswith("2025-09-30")
    for column, values in {
        "B": [0, .1, .15, .2, -.1],
        "C": [0, .1, .15, .2, -.1],
        "D": [.05, .1, .15, .2, -.05],
    }.items():
        for row, value in enumerate(values, 3):
            cell = inputs[f"{column}{row}"]
            assert cell.data_type == "n" and cell.value == value
            assert cell.number_format == "0.00%"
    rules = {str(rule.sqref): rule for rule in model.data_validations.dataValidation}
    assert set(rules) == {"C12", "C13", "C14"}
    for row, source in zip(range(12, 15), "BCD"):
        rule = rules[f"C{row}"]
        assert rule.type == "list" and rule.allowBlank
        assert rule.formula1.replace("'", "").lstrip("=") == f"Input!${source}$3:${source}$7"
        assert model[f"C{row}"].data_type == "n" and model[f"C{row}"].value == .15
        assert model[f"C{row}"].number_format == "0%"
    for col in range(4, 19):
        current, previous = get_column_letter(col), get_column_letter(col - 1)
        assert model[f"{current}2"].value == "Forecast"
        assert model[f"{current}3"].value == f"=EOMONTH({previous}3,1)"
        for row, assumption in [(4, 12), (5, 13), (8, 14)]:
            assert model[f"{current}{row}"].value == f"={previous}{row}*(1+$C{assumption})"
        assert model[f"{current}6"].value == f"={current}4-{current}5"
        assert model[f"{current}9"].value == f"={current}6-{current}8"
    for col in range(3, 19):
        label, date = model.cell(2, col), model.cell(3, col)
        assert label.font.name == date.font.name == "Arial"
        assert label.font.bold and label.font.sz == 9 and label.alignment.horizontal == "center"
        assert date.font.bold and date.font.sz == 10 and date.number_format == "m/d/yy"
        assert date.fill.fgColor.rgb[-6:].lower() == "b2dff3"
    assert model["B11"].font.bold
    for row in range(11, 15):
        for col in range(2, 4):
            assert model.cell(row, col).fill.fgColor.rgb[-6:].lower() == "ffff00"
    replay = json.loads((directory / "replay.json").read_text())
    final = next(step for step in replay if step["time"] == "9:55")
    cells = {cell["cell"]: cell for cell in final["cells"]}

    def currency(value):
        rounded = value.quantize(Decimal(1), rounding=ROUND_HALF_UP)
        return ("-$" if rounded < 0 else "$") + f"{abs(rounded):,.0f}"

    checked = 0
    for month in range(1, 16):
        factor = Decimal("1.15") ** month
        revenue, cost, opex = (Decimal(base) * factor for base in (4000000, 2500000, 2850000))
        for row, value in [(4, revenue), (5, cost), (6, revenue - cost), (8, opex), (9, revenue - cost - opex)]:
            address = f"{get_column_letter(month + 3)}{row}"
            expected = currency(value)
            assert cells[address]["display"] == expected, (address, cells[address]["display"], expected)
            checked += 1
    return {
        "status": "passed",
        "reader": "openpyxl",
        "nativeListRules": len(rules),
        "forecastMonths": 15,
        "finalDisplayedFinancialCellsComparedWithDecimalOracle": checked,
        "recordedReplayStates": len(replay),
        "finalAssumptions": [.15, .15, .15],
        "scope": "XLSX formulas, typed inputs, native list validation, formatting, and recorded UI text; Microsoft Excel execution and original palette RGB are unverified.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    report = verify(args.directory)
    (args.directory / "independent-inspection.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
