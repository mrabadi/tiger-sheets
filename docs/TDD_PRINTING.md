# Native worksheet printing

`printWorksheet(QWidget*, WorksheetModel&, QTableView&, const QString& pdfPath)` writes an actual Qt `QPrinter` PDF when a path is supplied, or opens `QPrintDialog` when it is empty. Canceling the dialog returns false. Invalid print ranges, rows taller than a page, and printer-start/page-creation failures raise descriptive errors. The view must display the supplied worksheet model.

The helper prints the first selected rectangle. A single selected/current cell uses the existing A1:I40 default. Ranges are bounded to 10,000 rows and 100 columns. These bounds and row-height feasibility are checked before starting PDF output, avoiding partial files for rejected selections.

Column widths and row heights come from the actual grid. A hidden section uses the corresponding default section size, so the frozen-row view does not remove the first printed row. The whole selected width shrinks uniformly when necessary to fit the printable page; smaller selections keep their normal size. Rows paginate without splitting or duplication. A row taller than a printable page is diagnosed with an instruction to reduce its height.

Rendering uses 96-DPI logical grid coordinates, converts cell fonts to pixel sizes in those coordinates, and applies the printer-resolution/fit transform to the complete table. This keeps font sizes, cell geometry, and column proportions consistent instead of mixing printer point units with screen cell dimensions. Fonts retain family/bold/italic/underline, with cell foreground/background colors and alignment. Text is clipped to its cell and rendered as a single line, matching the current grid. The helper reads displayed model values, including calculated results and number formatting; it neither recalculates nor modifies the workbook. The caller should commit pending editors and ensure calculation is current before invoking it.

Chart printing remains unsupported: this helper prints worksheet cells, not chart overlays. Repeating print titles, arbitrary print-area configuration, horizontal page splitting, merged-cell layout, and full Excel print settings are outside this bounded helper. No physical printer job was submitted during verification.

## Executed RED → GREEN evidence

1. The first selected-range/calculated-value test failed to compile because the deliberately new public `tigersheets/printing.hpp` did not exist. Added the API and actual Qt PDF output; the PDF header and independent `pdftotext` extraction passed, including `5` for `=2+3` and exclusion of an unselected cell.
2. Added an independent PDF bounding-box test. It failed with `printed columns must retain the grid's 80:480 proportions`. Implemented actual grid widths, logical-coordinate fonts and styles. Bounding boxes then verified both column proportions and the relative heights of 24pt and 11pt text; the complete large-font heading remained extractable.
3. Added a 150-row PDF test. After repairing a missing test include, its meaningful runtime RED was `pagination must print every selected row exactly once`. Added page breaks; first, middle, and last markers each appeared exactly once across multiple actual PDF pages.
4. Added oversized-selection tests. RED: `oversized selections must fail before opening the PDF output`. Added the 10,000-row/100-column guard; both limits now fail without creating a file.
5. Added an exceptionally tall-row regression. RED: `a row too tall to paginate must be diagnosed before PDF creation`. Preflighted row heights before opening output; the regression passed.

Additional passing checks exercise the default 40-by-9 range with a total width larger than the page, native print-dialog cancellation through its real modal event loop, unwritable PDF destinations, and unchanged source snapshots.

## Verification

The standalone verification project at `/tmp/tiger-printing-cmake` compiles the actual printing, grid, session, reference, and calculation sources and links Qt Widgets/PrintSupport plus the pinned calculation archive. It uses `-Wall -Wextra -Wpedantic`. The printing implementation and tests compile without warnings; existing grid formatting warnings were not changed.

```sh
cmake --build /tmp/tiger-printing-build --parallel 2
ctest --test-dir /tmp/tiger-printing-build --output-on-failure
```

Result: CTest 1/1 passed in 0.25 seconds, covering six test groups. `pdftotext` is an existing test dependency; no packages or global tools were installed. The implementation library needs `tigersheets_grid`, Qt Widgets, and Qt PrintSupport. Its tests also need `tigersheets_calculation`. MainWindow routing and root CMake integration are owned by the root agent and were not edited here.
