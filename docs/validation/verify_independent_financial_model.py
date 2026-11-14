"""Read-only external-library verification; never imports TigerSheets code or saves XLSX/PDF."""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import platform
import subprocess
import sys
import warnings
from datetime import datetime, timezone
from decimal import Decimal, localcontext
from pathlib import Path

import openpyxl
from openpyxl.utils import get_column_letter


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def command(args: list[str]) -> dict:
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    return {"argv": args, "exit_code": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixtures", default="build-release/fixtures")
    parser.add_argument("--report", default="docs/validation/independent-financial-model.json")
    parser.add_argument("--reviewed-render-sha256", default="")
    args = parser.parse_args()
    fixtures = Path(args.fixtures).resolve()
    files = {kind: fixtures / f"financial-model.{kind}" for kind in ("xlsx", "csv", "pdf")}
    original = {kind: path.read_bytes() for kind, path in files.items()}
    checks: list[dict] = []
    notes: list[str] = []

    def check(name: str, passed: bool, details=None) -> None:
        checks.append({"name": name, "passed": bool(passed), "details": details})

    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        workbook = openpyxl.load_workbook(io.BytesIO(original["xlsx"]), data_only=False)
        cached = openpyxl.load_workbook(io.BytesIO(original["xlsx"]), data_only=True)
    sheet = workbook.active
    csv_rows = list(csv.reader(io.StringIO(original["csv"].decode("utf-8-sig"), newline="")))
    check("one native worksheet", workbook.sheetnames == ["Sheet1"], workbook.sheetnames)
    formulas = {cell.coordinate: cell.value for row in sheet for cell in row if cell.data_type == "f"}
    expected_formulas = {}
    for col in range(4, 10):
        name = get_column_letter(col)
        previous = get_column_letter(col - 1)
        for row, formula in {6: f"={name}5*$B$6", 8: f"={name}6-{name}7",
                             9: f"={name}8*$B$8", 10: f"={name}8-{name}9"}.items():
            expected_formulas[f"{name}{row}"] = formula
        if col > 4:
            expected_formulas[f"{name}5"] = f"={previous}5*(1+$B$5)"
            expected_formulas[f"{name}7"] = f"={previous}7*(1+$B$7)"
    expected_formulas["D12"] = "=NPV($B$9,E10:I10)+(I10*(1+$B$10)/($B$9-$B$10))/(1+$B$9)^5"
    for index in range(5):
        col = get_column_letter(index + 5)
        expected_formulas[f"B{21 + index}"] = f"={col}5"
        expected_formulas[f"C{21 + index}"] = f"={col}8"
    check("all 45 native formulas retain expected references", formulas == expected_formulas,
          {"actual_count": len(formulas), "expected_count": len(expected_formulas)})
    formula_caches = {address: cached.active[address].value for address in formulas}
    check("CSV contains numeric formula results", all(csv_rows[sheet[address].row-1][sheet[address].column-1]
          and not csv_rows[sheet[address].row-1][sheet[address].column-1].startswith("=")
          for address in formulas))

    assumptions = {key: Decimal(str(sheet[address].value)) for key, address in {
        "revenue_growth": "B5", "gross_margin": "B6", "expense_growth": "B7",
        "tax_rate": "B8", "discount_rate": "B9", "terminal_growth": "B10",
        "base_revenue": "D5", "base_operating_expenses": "D7"}.items()}
    numeric_comparisons = []
    expected_values = {}
    cash_flows = []
    with localcontext() as context:
        context.prec = 50
        for year in range(6):
            col = get_column_letter(year + 4)
            revenue = assumptions["base_revenue"] * (1 + assumptions["revenue_growth"]) ** year
            expenses = assumptions["base_operating_expenses"] * (1 + assumptions["expense_growth"]) ** year
            gross_profit = revenue * assumptions["gross_margin"]
            ebitda = gross_profit - expenses
            taxes = ebitda * assumptions["tax_rate"]
            fcf = ebitda - taxes
            for row, value in {5: revenue, 6: gross_profit, 7: expenses, 8: ebitda, 9: taxes, 10: fcf}.items():
                expected_values[f"{col}{row}"] = value
            if year:
                cash_flows.append(fcf)
        discounted_fcf = sum(fcf / (1 + assumptions["discount_rate"]) ** (year + 1)
                             for year, fcf in enumerate(cash_flows))
        terminal_value = cash_flows[-1] * (1 + assumptions["terminal_growth"]) / (
            assumptions["discount_rate"] - assumptions["terminal_growth"])
        discounted_terminal_value = terminal_value / (1 + assumptions["discount_rate"]) ** 5
        expected_dcf = discounted_fcf + discounted_terminal_value
        expected_values["D12"] = expected_dcf
        for index in range(5):
            col = get_column_letter(index + 5)
            expected_values[f"B{21+index}"] = expected_values[f"{col}5"]
            expected_values[f"C{21+index}"] = expected_values[f"{col}8"]
        for address, expected in expected_values.items():
            cell = sheet[address]
            raw = csv_rows[cell.row-1][cell.column-1]
            actual = Decimal(raw)
            difference = abs(actual - expected)
            numeric_comparisons.append({"cell": address, "csv": raw, "independent_decimal": str(expected),
                                        "absolute_difference": str(difference), "passed": difference <= Decimal("0.000001")})
    check("CSV forecast and DCF agree with independent Decimal cash-flow arithmetic",
          all(value["passed"] for value in numeric_comparisons), {"cells_checked": len(numeric_comparisons), "absolute_tolerance": "0.000001"})

    def style(address: str) -> dict:
        cell = sheet[address]
        return {"number_format": cell.number_format, "bold": cell.font.bold, "font_size": cell.font.sz,
                "font_rgb": cell.font.color.rgb if cell.font.color and cell.font.color.type == "rgb" else None,
                "fill_rgb": cell.fill.fgColor.rgb, "horizontal_alignment": cell.alignment.horizontal}

    styles = {address: style(address) for address in ("B5", "D4", "D8", "D12")}
    check("input percentage style", styles["B5"]["number_format"] == "0.0%" and styles["B5"]["font_rgb"].upper() == "FF286A9E", styles["B5"])
    check("forecast header style", styles["D4"]["bold"] and styles["D4"]["fill_rgb"].upper() == "FF245648" and styles["D4"]["horizontal_alignment"] == "right", styles["D4"])
    check("enterprise value style", styles["D12"]["bold"] and styles["D12"]["font_size"] == 14 and styles["D12"]["number_format"] == "$#,##0" and styles["D12"]["fill_rgb"].upper() == "FFDFECE3", styles["D12"])
    charts = []
    for chart in sheet._charts:
        title = "".join(run.t for paragraph in chart.title.tx.rich.p for run in paragraph.r)
        series = [{"title_reference": series.tx.strRef.f, "categories_reference": series.cat.strRef.f,
                   "values_reference": series.val.numRef.f, "values_cache_present": series.val.numRef.numCache is not None}
                  for series in chart.series]
        charts.append({"class": type(chart).__name__, "type": chart.type, "title": title, "series": series,
                       "anchor_zero_based": {"row": chart.anchor._from.row, "col": chart.anchor._from.col}})
    expected_series = [{"title_reference": f"'Sheet1'!${col}$20", "categories_reference": "'Sheet1'!$A$21:$A$25",
                        "values_reference": f"'Sheet1'!${col}$21:${col}$25", "values_cache_present": False}
                       for col in ("B", "C")]
    check("native column chart has two correctly linked series", len(charts) == 1 and charts[0]["type"] == "col"
          and charts[0]["title"] == "Revenue & EBITDA forecast" and charts[0]["series"] == expected_series, charts)

    pdfinfo = command(["/usr/bin/pdfinfo", str(files["pdf"])])
    pdftext = command(["/usr/bin/pdftotext", "-layout", str(files["pdf"]), "-"])
    render_prefix = Path("/tmp/tigersheets-independent-financial-model-page1")
    render = command(["/usr/bin/pdftoppm", "-f", "1", "-singlefile", "-scale-to", "1800", "-png", str(files["pdf"]), str(render_prefix)])
    render_path = render_prefix.with_suffix(".png")
    render_hash = sha256(render_path.read_bytes()) if render["exit_code"] == 0 else None
    check("PDF independently parses and renders", pdfinfo["exit_code"] == pdftext["exit_code"] == render["exit_code"] == 0)
    check("PDF is one page", "Pages:           1" in pdfinfo["stdout"])
    normalized_pdf = " ".join(pdftext["stdout"].split())
    check("PDF exposes the displayed valuation and model sections", all(text in normalized_pdf for text in
          ("FINANCIAL MODEL", "ASSUMPTIONS", "Enterprise value", "$6,301,151", "CHART DATA")))
    reviewed = bool(args.reviewed_render_sha256 and render_hash == args.reviewed_render_sha256)
    check("current PDF render matches manually reviewed image", reviewed,
          {"render_sha256": render_hash, "reviewed_sha256": args.reviewed_render_sha256})
    notes.extend([
        "Microsoft Excel was not run; native Excel opening, recalculation, chart rendering and keyboard/VBA interoperability remain unverified.",
        "openpyxl loads formula definitions and chart/style structures but does not calculate formulas.",
        "XLSX formula caches are absent (including D12); consumers need recalculation. The workbook requests fullCalcOnLoad.",
        "XLSX has no explicit native column widths; the application's adjusted screen widths are not encoded in this fixture.",
        "Manual PDF review shows a legible cell table and chart-data table, but the native chart graphic is absent from the PDF.",
        "pdftotext loses some ligature characters; the rendered PDF text is legible.",
        "System /usr/bin Poppler was used because the bundled pdfinfo binary requires unavailable GLIBC_2.38.",
    ])
    stable = all(path.read_bytes() == original[kind] for kind, path in files.items())
    check("fixture bytes remained unchanged during verification", stable)
    report = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "passed_with_limitations" if all(item["passed"] for item in checks) else "failed",
        "independent_tools": {"python": platform.python_version(), "python_executable": sys.executable,
                              "openpyxl": openpyxl.__version__, "pdftotext_version": command(["/usr/bin/pdftotext", "-v"])},
        "inputs": {kind: {"path": str(files[kind]), "bytes": len(data), "sha256": sha256(data)} for kind, data in original.items()},
        "checks": checks, "limitations": notes,
        "xlsx": {"sheet_names": workbook.sheetnames, "dimensions": sheet.calculate_dimension(), "formulas": formulas,
                 "formula_caches": formula_caches, "fullCalcOnLoad": workbook.calculation.fullCalcOnLoad,
                 "explicit_column_widths": {name: dimension.width for name, dimension in sheet.column_dimensions.items()},
                 "styles": styles, "charts": charts, "openpyxl_warnings": [str(item.message) for item in caught]},
        "csv": {"row_count": len(csv_rows), "column_counts": sorted({len(row) for row in csv_rows}),
                "DCF_cell": "D12", "DCF_raw_numeric": csv_rows[11][3], "numeric_comparisons": numeric_comparisons},
        "independent_cash_flow": {"assumptions": {name: str(value) for name, value in assumptions.items()},
                                  "forecast_fcf": [str(value) for value in cash_flows], "discounted_fcf": str(discounted_fcf),
                                  "terminal_value": str(terminal_value), "discounted_terminal_value": str(discounted_terminal_value),
                                  "enterprise_value": str(expected_dcf)},
        "pdf": {"pdfinfo": pdfinfo, "pdftotext": pdftext, "render": render,
                "render_path": str(render_path), "render_sha256": render_hash, "manually_reviewed": reviewed,
                "manual_visual_notes": "Legible labels, assumptions, forecast, valuation and chart data; no obvious overlap or clipping. Chart graphic not printed." if reviewed else "Review pending for current render."},
    }
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "passed_with_limitations" else 1


if __name__ == "__main__":
    raise SystemExit(main())
