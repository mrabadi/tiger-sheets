#!/usr/bin/env python3
"""Read-only comparison of PDF glyph extraction and tagged ActualText corpora."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "docs/validation/otis-90-minute"
TERMS = ["growth", "urbanization", "increase", "development", "productivity"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def count(text, term):
    term = re.escape(term)
    return {"substring": len(re.findall(term, text, re.I)), "wholeWord": len(re.findall(r"\b" + term + r"\b", text, re.I)),
            "caseSensitive": {"lower": {"substring": len(re.findall(term, text)), "wholeWord": len(re.findall(r"\b" + term + r"\b", text))},
                              "title": {"substring": len(re.findall(term.capitalize(), text)), "wholeWord": len(re.findall(r"\b" + term.capitalize() + r"\b", text))},
                              "upper": {"substring": len(re.findall(term.upper(), text)), "wholeWord": len(re.findall(r"\b" + term.upper() + r"\b", text))}}}


def inspect(path, terms):
    import pikepdf
    original = sha(path)
    pdf = pikepdf.Pdf.open(path)
    corpora = {}
    for name, arguments in [("PopplerLayout", ["-layout"]), ("PopplerRaw", ["-raw"])]:
        text = subprocess.check_output(["/usr/bin/pdftotext", *arguments, str(path), "-"]).decode()
        corpora[name] = {term: count(text, term) for term in terms}
    try:
        from pdfminer.high_level import extract_text
        text = extract_text(str(path))
        corpora["PDFMiner"] = {term: count(text, term) for term in terms}
    except ImportError:
        corpora["PDFMiner"] = {"available": False}
    page_numbers = {page.obj.objgen: number + 1 for number, page in enumerate(pdf.pages)}
    spans, relevant = [], []
    for obj in pdf.objects:
        if isinstance(obj, pikepdf.Dictionary) and "/ActualText" in obj:
            text = str(obj["/ActualText"])
            spans.append(text)
            if any(re.search(re.escape(term), text, re.I) for term in terms):
                page = obj.get("/Pg")
                relevant.append({"object": list(obj.objgen), "page": page_numbers.get(page.objgen) if page is not None else None,
                                 "role": str(obj.get("/S", "")), "text": text,
                                 "counts": {term: count(text, term) for term in terms}})
    corpora["ActualTextPerSpan"] = {}
    for term in terms:
        counts = [count(text, term) for text in spans]
        corpora["ActualTextPerSpan"][term] = {"substring": sum(item["substring"] for item in counts), "wholeWord": sum(item["wholeWord"] for item in counts),
            "caseSensitive": {case: {mode: sum(item["caseSensitive"][case][mode] for item in counts) for mode in ["substring", "wholeWord"]} for case in ["lower", "title", "upper"]}}
    operators = Counter()
    for page in pdf.pages:
        for instruction in pikepdf.parse_content_stream(page):
            if str(instruction.operator) == "Tr":
                operators[str(instruction.operands[0])] += 1
    assert sha(path) == original, "Read-only inspection changed the original PDF"
    return {"pdf": str(path), "sha256": original, "bytes": path.stat().st_size, "pages": len(pdf.pages),
            "metadata": {str(key): str(value) for key, value in pdf.docinfo.items()},
            "tagged": bool(pdf.Root.get("/MarkInfo", {}).get("/Marked", False)), "optionalContentPropertiesPresent": "/OCProperties" in pdf.Root,
            "pageContentTextRenderingModeOperators": dict(operators), "ActualTextSpanCount": len(spans),
            "corpora": corpora, "relevantActualTextObjects": relevant, "originalUnchanged": True,
            "limits": ["Per-span whole-word boundaries are not Adobe WordFinder's geometry/tag-aware word boundaries.",
                       "The ActualText corpus is an actual second text representation; its presence does not prove Acrobat indexed it twice.",
                       "No Acrobat engine, cloud synonym service or historical PDF revision was executed.",
                       "Rendering-mode scan covers direct page content streams; it is not a proof that every nested form/image is free of hidden text.",
                       "No duplicate multiplier or query-specific source count is applied by this checker."],
            "primaryDocumentation": [
                {"url": "https://opensource.adobe.com/dc-acrobat-sdk-docs/acrobatsdk/apireference/PD_Layer/PDWordFinder.html", "supports": "Adobe exposes distinct tagged-PDF handling and configurable word-breaking behavior."},
                {"url": "https://helpx.adobe.com/acrobat/using/searching-pdfs.html", "supports": "Adobe describes matching-word suggestions and frequency, plus whole-word and case-sensitive search options."}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf", type=Path, nargs="?", default=BASE / "reference/101-01-Otis-Investor-Presentation.pdf")
    parser.add_argument("--output", type=Path, default=BASE / "pdf-search-index-investigation.json")
    parser.add_argument("--terms", nargs="+", default=TERMS)
    args = parser.parse_args()
    report = inspect(args.pdf.resolve(), args.terms)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"report": str(args.output.resolve()), "originalUnchanged": True, "ActualTextSpanCount": report["ActualTextSpanCount"],
                      "counts": {name: {term: {key: result[term][key] for key in ["substring", "wholeWord"]} for term in args.terms} for name, result in report["corpora"].items() if "available" not in result}}))


if __name__ == "__main__":
    main()
