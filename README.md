# TigerSheets

TigerSheets is an independent C++20/Qt 6 spreadsheet application for Ubuntu. It runs locally and uses a Rust static-library adapter around the pinned IronCalc engine. Its native workspace and faceted tiger artwork follow OwlDocs' suite styling. No LibreOffice or ONLYOFFICE component is used.

**Status: implementation in progress; the requested complete Excel-compatible release has not passed acceptance.** The executable can be built and used for development. Full-release packaging intentionally fails while required catalog entries lack passing evidence. A clearly labeled development installer with worksheet context menus is available for local evaluation. An available function name is not evidence of complete Excel semantics.

## Install on Ubuntu

The current Ubuntu amd64 development version is `0.3.0~dev.4`. Build the application and development installer using the commands below; the resulting package is `build-release/tiger-sheets_0.3.0~dev.4_amd64.deb`. The latest changes and test evidence are recorded in [worksheet interaction validation](docs/TDD_WORKSHEET_INTERACTIONS.md).
From this repository directory:

```sh
sudo apt install ./build-release/tiger-sheets_0.3.0~dev.4_amd64.deb
```

Right-click cells or row/column headers for [worksheet context menus](docs/CONTEXT_MENUS.md), including merge/unmerge, insert/delete, Format Cells, sizing and hide/unhide. Use **Data → Forecast** for [forecasts and what-if workflows](docs/FORECAST_WORKFLOWS.md).

The current source build also supports draggable fill handles, persistent column auto-fit, repeated Paste into selected cells, and **Data → Data Validation** range-backed list dropdowns. The original financial-model tutorial walkthrough is retained as [historical evidence](docs/VIDEO_FINANCIAL_MODEL_VALIDATION.md); its replay test has been removed.

The [Otis tutorial exercise](docs/OTIS_TUTORIAL_VALIDATION.md) completed 9,952 top-level / 10,514 recursive actions as a standalone feature exercise. Full video replication remains outside built-in regression tests.

All three follow-up areas are complete: native UI behavior, workbook formatting and audit consistency. The local artifact `output/otis-90-minute/Otis-TigerSheets-Corrected.zip` contains the repaired model, linked Raw source and compact results; generated packages are excluded from Git. Four saved-file audits and both native reopen/refresh phases passed, preserving all 891 stored cell contents and 611 formulas/caches. Before cleanup, validation recorded 273 Python passes and passing evidence for 280 native tests: 279 initial passes plus the unchanged timed-out driver's serial pass. These suites were not rerun after cleanup. See the [results summary](docs/validation/otis-90-minute/RESULTS.json).

Generated replay outputs, screenshots, logs, raw audit inventories and obsolete packages were removed at the user's request. Source and required input fixtures remain; installers exclude `docs/validation`. Helvetica display is accepted. No fresh full-video rerun or exhaustive pixel identity is claimed; `sourceReviewComplete` and `verifiedReplica` remain false.

Then launch **TigerSheets** from the application menu, or run `tiger-sheets --demo`.
APT installs the required Qt platform packages and other runtime dependencies.
This is a development app; complete Excel/VBA compatibility remains unfinished.
See [installation and package validation](docs/INSTALLATION.md).

## Run the current application

The optimized application target in this checkout is `build-release/tiger-sheets`. After building in that directory:

```sh
./build-release/tiger-sheets
./build-release/tiger-sheets --demo
./build-release/tiger-sheets /absolute/path/to/model.xlsx
```

Use **File → Financial model example** for the editable forecast and DCF. The Data ribbon contains **PivotTable**, **Convert summary…**, Goal Seek, Solver, scenarios and sensitivity tables. Native reporting supports row/column/filter fields, multiple measures, date grouping, explicit refresh and linked line/column/bar PivotCharts. See the [reporting guide](docs/REPORTING_USER_GUIDE.md). Developer opens the supported VBA editor and recorder. Codex connects only through explicit interaction in its panel; ordinary editing works without it.

Formula entry now offers available functions as you type in either the cell or formula bar. Up/Down selects a suggestion and Tab inserts it with its opening parenthesis. `=AVG` suggests canonical `AVERAGE` functions. Argument hints follow nested calls. Defined names and enabled workbook functions are included. Click/drag cells or use arrows and Shift to build references; F2 switches between pointing and moving the text cursor, and F4 cycles absolute/mixed references. Enter accepts and moves down; Tab accepts and moves right when no completion is open. Escape dismisses formula help, then cancels the draft.

Copy and Cut show an animated dashed source border. Escape cancels copy mode; repeated Paste retains the source border. Right-click a worksheet tab for Rename… or Delete, or double-click a tab to rename it. Deletion asks for confirmation and supports Undo.

Selecting cells, rows or columns preserves their formatting under a light tint and outlines the selected range. The workbook header shows **Not saved**, **Unsaved changes** or **Saved locally**, with a matching native modified-title indicator. A successful save clears the modified status; cancelled saves and CSV exports keep source-workbook changes marked unsaved.

Calculate now and worksheet switching retain typed numeric results, percentages/dates and Values-paste behavior. Show formulas keeps numeric selection totals and literal number formats. Reporting headers can use calculated formula names.

Use **Home → Format…** or **Ctrl+1** for number-format options: decimal places, thousands separators, currency symbols, minus signs or parentheses for negative currency, percentage precision, and date display choices. The Sample shows the result before applying it; rounding changes display without changing the stored value. Entered numeric dates retain their input format, and dragging the fill handle copies cell formatting along with values or adjusted formulas. The Home format selector follows the active cell. Selecting a formula cell outlines its local cell references; cell editing stays inside the cell.

Click a chart to select it. Double-click or F2 opens its title, type and data-range editor; right-click exposes the same commands. Drag to move, use a selected corner to resize, and Escape cancels a gesture. Delete removes the selected chart. Edits, movements and resizes are undoable and persist through XLSX saves. Selecting another chart or returning to cells clears the previous handles.

Executed reporting and forecast qualification remains in the [reporting baseline](docs/RELEASE_VALIDATION.md) and [forecast validation](docs/FORECAST_RELEASE_VALIDATION.md). The current [context-menu validation](docs/CONTEXT_MENU_VALIDATION.md) records the new regression tests, installer audit and remaining limits; older performance/platform evidence is not a fresh qualification of this revision.

Codex now has OwlDocs-style Model, Reasoning and Speed selectors. It can propose financial data, formulas, formatting and native editable charts together. Related previews accumulate, and reads/searches inspect pending edits with calculated values. Accept applies the proposal as one undoable edit; saving remains a desktop action. An authenticated live fictional-data reporting workflow passed native PivotTable/PivotChart creation, reviewed acceptance, Undo/Redo and XLSX save/reopen; see [the bounded live acceptance record](docs/validation/reporting-live/acceptance.json). This qualifies that recorded workflow, not every model or tool path. See [using Codex](docs/CODEX_INTEGRATION.md).

The current implementation includes sparse Excel-sized worksheets, revision-checked transactions, undo/redo, isolated proposals, XLSX/XLSM package preservation, CSV, native chart rendering, three numerical optimization backends, local recovery, and a cancellable macro worker. These implementations have limits described in `docs/TDD_*.md`; they do not establish full release compatibility.

Imported unchanged VBA projects are preserved. **Saving new or edited native VBA projects is blocked.** Edited source remains in recovery; the independent runtime currently implements a subset of the language and workbook object model. New supported PivotTables and PivotCharts are authored as genuine native XLSX definitions, typed caches and linked chart parts. Older static summaries remain readable and can be explicitly converted through a preview on a new worksheet while retaining their original cells. Sensitivity results remain ordinary worksheet cells; native Excel data-table records and full pivot interoperability remain incomplete.

## Build on Ubuntu 22.04 amd64

Build prerequisites: CMake 3.22+, Ninja, a C++20 compiler, Qt 6.2+ Core/Gui/Widgets/Test/PrintSupport development files, nlohmann-json, SQLite3, libzip, pugixml, libseccomp, Rust/Cargo, and the exact solver libraries in `third_party/SOLVER_DEPS.json`. See [dependency instructions](docs/DEPENDENCIES.md) for the local dependency prefix. Rust's `Cargo.lock` and the vendored engine pin are checked into the source tree.

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTIGERSHEETS_CARGO=/absolute/path/to/cargo \
  -DTIGERSHEETS_SOLVER_PREFIX="$PWD/.deps/solvers/prefix"
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure
```

An isolated Rust installation can additionally set `TIGERSHEETS_CARGO_HOME` and `TIGERSHEETS_RUSTUP_HOME`. The native build invokes Cargo with `--offline --locked`; the 72 registry dependencies are vendored with verified checksums, so no populated registry cache is required. See [Rust dependency verification](docs/RUST_DEPENDENCIES.md). No toolchain or system configuration is installed by CMake.

For C++-only development with an existing calculation archive, configure `TIGERSHEETS_PREBUILT_CALC_ARCHIVE` with its absolute path. That mode does not rebuild or verify Rust source compilation; the current worksheet-interaction validation used it.

## Compatibility and validation

`compatibility/functions.json` and `compatibility/shortcuts.json` freeze Microsoft's reviewed function and Windows shortcut references. `compatibility/features.json` records the release features. The catalogs retain excluded entries and reasons. Function fixtures use independent documented expectations, with executed results written to `build-release/formula-evidence.json`.

```sh
python3 tools/release_gate.py --report build-release/formula-evidence.json
cd build-release
cpack -G DEB
```

Both full-release commands currently report **RELEASE BLOCKED**. To build a local development installer with an explicit prerelease version and incomplete-compatibility description, use:

```sh
cpack --config build-release/CPackDevelopmentConfig.cmake -B dist
```

The default CPack configuration continues to enforce release acceptance. The release needs complete semantic and interaction coverage, native VBA authoring/debugging/UserForms/events, full chart/pivot interoperability, structural reference editing, calculation/date compatibility, performance, accessibility, clean-install/upgrade, printing, X11 and Wayland acceptance. See [validation status](docs/RELEASE_VALIDATION.md).

Tests and reference verification run on Ubuntu only. Actual Microsoft Excel reopening and VBA execution are unverified. The program must not be described as complete Excel parity on the strength of this test suite.

## Source layout

- `src/session.cpp`: authoritative workbook, transactions, previews and history.
- `src/ooxml.cpp`: owned SpreadsheetML package reader/writer and atomic saves.
- `rust/ironcalc_bridge` and `third_party/ironcalc`: C ABI and pinned calculation source.
- `src/grid.cpp`, `src/main_window.cpp`, `src/chart.cpp`: native desktop workspace.
- `src/pivot.cpp`, `src/pivot_ooxml.cpp`, `src/pivot_fields_pane.cpp`, `src/report_worker_main.cpp`: native reporting, fields and isolated refresh.
- `src/modeling_ui.cpp`, `src/solver.cpp`: isolated analysis and accepted result transactions.
- `src/vba.cpp`, `src/vba_ui.cpp`, `src/vba_worker_main.cpp`: independent VBA subset and worker.
- `src/codex.cpp`, `src/codex_controls.cpp`, `src/workbook_tools.cpp`, `schemas/workbook.v1.json`: optional app-server integration and isolated workbook authoring.
- `src/recovery.cpp`: SQLite recovery with original package data and integrity checks.

Third-party dependency licenses and pins are retained under `third_party`; project distribution licensing remains to be finalized before release.
