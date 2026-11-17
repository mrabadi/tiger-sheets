# BIFF regression fixtures

These small files exercise native spreadsheet import and persistence. They are data inputs, not a video replay or a constructed tutorial solution.

- `independent.xls` and `allocation-budget.xls` are synthetic BIFF8 files produced by `generate.py` using xlwt 1.3.0. Regeneration needs xlwt; the C++ tests do not.
- `financial-source.xls` is the unchanged 120-sheet financial-statement BIFF resource linked by the official tutorial resource page, [3-statement model](https://mergersandinquisitions.com/3-statement-model/). Its [original download](https://youtube-breakingintowallstreet-com.s3.amazonaws.com/101-01-Otis-10-K-Raw.xls) contains historical Otis financial data. Size: 337,920 bytes. SHA-256: `08cabfcfd01213bc588176387be36b7e3610267c9633987e90b404fc0ed492f4`.
- `financial-source-reference.json` is the independently parsed xlrd 2.0.2 expectation for that file: 120 sheets, 3,440 nonempty cells and 15,527 formatted blanks. SHA-256: `9398114caf5c618cc9c692987a3090d93d682cda5e19374eefcb1f174e993847`. Its `source` field retains the original local provenance path; tests load the sibling fixture instead.
- `accounting-formats.xml` contains only the two zero-decimal Accounting number-format records (41 and 42) extracted from `xl/styles.xml` of the [published completed workbook](https://youtube-breakingintowallstreet-com.s3.amazonaws.com/101-01-90-Minute-Case-Study-Otis-Complete.xlsx), whose SHA-256 is `7e7b7967312980c8e57772873764810a2df47ebe77264f1e6de2168d9562412e`. It supplies an independent format-code expectation without including the completed model.

The full tutorial workbooks, video, PDFs, screenshots, replay manifests and historical run archives are local exercise inputs and outputs. They are not required by the built-in product tests or published with these fixtures. The separate manual exercise tools expect their local validation data to be supplied explicitly.
