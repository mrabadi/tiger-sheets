#!/usr/bin/env python3
"""Independent read-only acceptance inspector; never imports TigerSheets code.

Requires openpyxl. Ordinary formulas are checked as formulas, because TigerSheets
writes them without Excel's cached <v> results. Numeric comparison/sensitivity
outputs are independently verified against the small fixture's arithmetic.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET
import openpyxl

S = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def near(actual, expected: float, message: str) -> None:
    check(isinstance(actual, (int, float)) and not isinstance(actual, bool)
          and math.isfinite(actual) and abs(actual-expected) < 1e-7, message)


def inspect(path: Path) -> dict:
    checked_numeric = 0
    with zipfile.ZipFile(path) as archive:
        check(archive.testzip() is None, "ZIP integrity")
        xml_parts = [name for name in archive.namelist()
                     if name.endswith((".xml", ".rels"))]
        for name in xml_parts:
            ET.fromstring(archive.read(name))
        metadata = json.loads(archive.read("tigersheets/metadata.json"))
        for part, digest in metadata["fingerprints"].items():
            check(hashlib.sha256(archive.read(part)).hexdigest() == digest,
                  f"supplemental metadata fingerprint: {part}")
        check(set(metadata["sheetIdentities"].values()) == {
            "model", "forecast", "comparison", "one", "two", "finance-inputs",
            "circular-financing"}, "stable supplemental worksheet identities")
        definitions = metadata["metadata"]["financialModels"]
        check(len(definitions) == 11, "all saved setups and generated reports")
        check({item["kind"] for item in definitions} == {
            "forecast", "assumption_set", "scenario", "scenario_comparison",
            "sensitivity", "goal_seek", "solver"}, "all seven workflow families")
        by_id = {item["id"]: item for item in definitions}
        check(by_id["base"]["cells"][3]["type"] == "string"
              and by_id["base"]["cells"][3]["value"] == "00123",
              "saved assumptions preserve leading-zero text")
        check(by_id["base"]["cells"][4]["type"] == "boolean"
              and by_id["base"]["cells"][4]["value"] is True,
              "saved assumptions preserve boolean")
        check(by_id["base"]["cells"][5]["type"] == "blank"
              and by_id["base"]["cells"][5]["value"] is None,
              "saved assumptions preserve blank")
        check(by_id["profit-target"]["specification"]["target"] == 200,
              "Goal Seek setup target remains saved")
        check(by_id["profit-solver"]["specification"]["method"] == "linear"
              and by_id["profit-solver"]["specification"]["variables"][0]["upper"] == .5,
              "Solver setup retains method and bounds")
        workbook_xml = ET.fromstring(archive.read("xl/workbook.xml"))
        calc = workbook_xml.find(S+"calcPr")
        check(calc is not None and calc.attrib.get("iterate") in ("1", "true")
              and int(calc.attrib["iterateCount"]) == 200,
              "native OOXML iteration settings")
        near(float(calc.attrib["iterateDelta"]), 1e-10, "native iteration threshold")
        formula_count = 0
        cached_formula_count = 0
        for name in xml_parts:
            if name.startswith("xl/worksheets/") and name.endswith(".xml"):
                for cell in ET.fromstring(archive.read(name)).iter(S+"c"):
                    if cell.find(S+"f") is not None:
                        formula_count += 1
                        value = cell.find(S+"v")
                        cached_formula_count += int(value is not None and value.text is not None)
    workbook = openpyxl.load_workbook(path, data_only=False)
    values = openpyxl.load_workbook(path, data_only=True)
    check(workbook.sheetnames == ["Operating model", "Forecast", "Scenario comparison",
                                 "One variable", "Two variables", "Finance assumptions",
                                 "Circular financing"], "standard worksheet names and order")
    model = workbook["Operating model"]
    for address, expected in {"B2": 1000, "B3": .1, "B4": .2, "B5": 100}.items():
        near(model[address].value, expected, f"unchanged source input {address}")
    check(model["B6"].value == "00123" and model["B6"].data_type == "s",
          "native leading-zero text")
    check(model["B7"].value is True and model["B7"].data_type == "b", "native boolean")
    check(model["B8"].value is None, "native blank")
    check(model["B10"].value == "=B2*(1+B3)" and model["B11"].value == "=B10*B4-B5",
          "unchanged source formulas")
    near(1000*(1+.1)*.2-100, 120, "independent source model profit")
    forecast = workbook["Forecast"]
    check([forecast.cell(1, col).value for col in range(1, 7)] ==
          ["Driver", "Base", "Growth", "2027", "2028", "2029"], "forecast headers")
    for address, formula in {"B2": "='Operating model'!$B$2",
                             "C2": "='Operating model'!$B$3", "D2": "=$B2*(1+$C2)",
                             "E2": "=D2*(1+$C2)", "F2": "=E2*(1+$C2)",
                             "D3": "=$B3*(1+$C3)", "F3": "=E3*(1+$C3)"}.items():
        check(forecast[address].value == formula, f"ordinary auditable formula {address}")
    near(1000*1.1**3, 1331, "independent forecast revenue")
    near(100*1.05**3, 115.7625, "independent forecast costs")
    for row, expected in enumerate([["Current model", 120, 1100],
                                    ["Base assumptions", 120, 1100],
                                    ["Upside", 260, 1440]], start=2):
        check(workbook["Scenario comparison"].cell(row, 1).value == expected[0],
              "scenario case order")
        for col in (2, 3):
            near(values["Scenario comparison"].cell(row, col).value, expected[col-1],
                 f"independent scenario value {row},{col}")
            checked_numeric += 1
    for row, (margin, profit) in enumerate([(.1, 10), (.2, 120), (.3, 230)], start=2):
        near(values["One variable"].cell(row, 1).value, margin, "one-variable input")
        near(values["One variable"].cell(row, 2).value, profit,
             "one-variable independently calculated profit")
        checked_numeric += 2
    for col, margin in enumerate((.1, .2, .3), start=2):
        near(values["Two variables"].cell(1, col).value, margin, "two-variable column")
        checked_numeric += 1
    for row, growth in enumerate((0, .1), start=2):
        near(values["Two variables"].cell(row, 1).value, growth, "two-variable row")
        checked_numeric += 1
        for col, margin in enumerate((.1, .2, .3), start=2):
            near(values["Two variables"].cell(row, col).value,
                 1000*(1+growth)*margin-100, "two-variable independent profit")
            checked_numeric += 1
    for sheet_name, addresses in {"Forecast": ("B2", "D2", "F3"),
                                  "Scenario comparison": ("B2", "C4"),
                                  "One variable": ("B2",), "Two variables": ("B2",)}.items():
        for address in addresses:
            check(workbook[sheet_name][address].number_format == "$#,##0.00",
                  f"native financial number format {sheet_name}!{address}")
    check(workbook["One variable"]["A2"].number_format == "0.0%"
          and workbook["Two variables"]["B1"].number_format == "0.0%",
          "sensitivity axis percent formats")
    circular = workbook["Circular financing"]
    check(circular["B6"].value == "=MAX(0,B3-B2+B7)"
          and circular["B7"].value == "=B6*B4", "circular financing ordinary formulas")
    near((150-100)/(1-.1), 50/.9, "independent converged debt")
    check(formula_count >= 16 and cached_formula_count == 0,
          "authored formulas are ordinary uncached OOXML formulas")
    return {"file": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": path.stat().st_size, "xmlPartsParsed": len(xml_parts),
            "financialDefinitions": len(definitions), "numericOutputChecks": checked_numeric,
            "ordinaryFormulas": formula_count, "cachedFormulaValues": cached_formula_count,
            "openpyxlVersion": openpyxl.__version__, "status": "passed",
            "qualification": "Read-only ZIP/XML/openpyxl inspection; Microsoft Excel application not run"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = {"status": "passed", "files": [inspect(path) for path in args.files]}
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(encoded+"\n")
    print(encoded)


if __name__ == "__main__":
    main()
