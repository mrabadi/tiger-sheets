# Financial-model video acceptance

This records the original tutorial acceptance run. The video replay has since
been removed from the automated test suite; focused regression tests cover the
individual spreadsheet and UI behaviors. The commands and results below are
historical evidence, rather than a replay test maintained by the current source.

Reference: [Create a Financial Model in 10 Minutes!](https://www.youtube.com/watch?v=DBNVhmI6WtQ), The Financial Controller. The [source checklist](validation/video-financial-model/video-checklist.md) contains 49 observed action/checkpoint descriptions, including uncertainties in the source recording.

## Result

The native Qt replay passes from the starting actuals through the presenter's last dropdown change. It checks the formulas, typed values, dates, percentage labels, financial outputs, and final cell formatting. It deliberately reproduces the initial flat revenue forecast, inserts the missing `$` in the existing formula, then verifies the corrected forecast.

The forecast extends through **December 2026**, not the September endpoint mentioned in the narration. The profitable 20%/15%/15% scenario displays **$101,617 in April 2026**. The last demonstrated change returns revenue growth to 15%; the final assumptions are **15%/15%/15%**, and October net income is **-$1,552,500**.

The independent openpyxl inspection passes: 3 native list rules, 15 forecast months, typed numeric assumptions, formulas, fonts/fills/alignment, and **75 final displayed financial cells** compared against an independent Decimal oracle. This complements the native application's calculations and save/reopen checks.

The final shared C++ build passed, and **all 133 CTest checks passed** after the last changes. The [execution summary](validation/video-financial-model/execution-summary.json), `full-build.log`, and `full-ctest.log` record the commands and results.

## Changes

- `WorksheetView`: visible draggable fill handle, horizontal/vertical copying with mixed references and source formatting, one-step undo, Escape cancellation, edge scrolling, and protection of previews/merged cells. Header-boundary double-click now persists auto-fit widths, including hidden-column state.
- Validation core/session/OOXML: bounded range-backed list definitions, typed and formatted options, undoable creation/replacement/removal, source-reference rename handling, metadata checks, and native XLSX data-validation read/write. Unsupported imported rules remain preserved.
- Validation UI: **Data → Data Validation → List**, source selection by switching sheets and dragging a range, dropdown arrow or Alt+Down, and protection against stale menu actions and cached results from another workbook.
- Cell entry and clipboard: percentages retain numeric fractions and their format; editing a percentage-formatted cell supports a whole percentage. Paste repeats a copied cell/range across a compatible selected rectangle in one undoable operation.
- Home ribbon: **%** applies whole-percentage formatting without changing the numeric value.

Production files changed: `src/worksheet_view.cpp`, `src/grid.cpp`, `src/main_window.cpp`, `src/session.cpp`, `src/ooxml.cpp`, the new `src/worksheet_validation.cpp` and `src/worksheet_validation_ui.cpp`, their headers, and `CMakeLists.txt`. Ten focused/acceptance C++ test targets and `tools/verify_video_financial_model.py` provide the executable coverage.

## Replay and inspection

From the repository root:

```sh
cmake --build build-release -j2
QT_QPA_PLATFORM=offscreen \
  TIGERSHEETS_VIDEO_OUTPUT_DIR="$PWD/docs/validation/video-financial-model" \
  build-release/video_financial_model_tests
python3 tools/verify_video_financial_model.py docs/validation/video-financial-model
ctest --test-dir build-release --output-on-failure -j2
```

The configured C++ development build explicitly uses the existing pinned calculation archive because the previous temporary Cargo toolchain is gone. The default source-build path remains available. This run does **not** verify a fresh Rust compilation; archive details and SHA-256 are in [the core evidence](validation/video-financial-model/validation-core-evidence.json).

To reproduce the tutorial interactively using the freshly built application:

```sh
./build-release/tiger-sheets docs/validation/video-financial-model/video-start.xlsx
```

Use `video-replicated.xlsx` in the same directory to inspect the completed result. `replay.json` records the application's cells, formulas, and displayed values at each recorded state. The replay also asserts final formatting and XLSX save/reopen behavior without generating screenshots.

## TDD evidence

Each added behavior was preceded by an executed failing test. The actual missing-behavior failures include:

| Behavior | RED evidence | GREEN evidence |
| --- | --- | --- |
| Drag-fill, mixed references, cancellation, scrolling, auto-fit | [detailed commands and preserved tool output](validation/video-financial-model/evidence.md) | fresh focused tests and final CTest run |
| Session list definitions and native XLSX persistence | `worksheet_validation_tests-red.log`, `worksheet_validation_ooxml_tests-red.log` | `validation-session-green.log`, `validation-ooxml-green.log` |
| Typed options, lookup, percentage literals, invalid rules, opaque XML, reference safety | `validation-*-red.log` | corresponding `validation-*-green.log` |
| Native setup and dropdown selection | `validation-ui-red.log`: command missing | `validation-ui-green.log` |
| Old dropdown after cell selection changes | `validation-stale-red.log`: previous cell changed | `validation-stale-green.log` |
| Old formula options after workbook replacement | `validation-context-red.log`: previous workbook's option offered | `validation-context-green.log` |
| Percentage entry | `percentage-red.log`: decimal shown | `percentage-green.log` |
| Literal percent in a custom number format | `percentage-quoted-red.log`: numeric entry rescaled | `percentage-quoted-green.log` |
| Canonical numeric dropdown options | `validation-canonical-option-red.log`: 100% became 1% | `validation-canonical-option-green.log` |
| Inherited percentage entry | `percentage-inherited-red.log`: 15 became 1500% | `percentage-inherited-green.log` |
| Repeated paste and destination bounds | `tiled-paste-red.log`, `tiled-paste-boundary-red.log` | `tiled-paste-green.log`, `tiled-paste-boundary-green.log` |
| Percent toolbar action | `percent-button-red.log`: action missing | `percent-button-green.log` |

Logs without paths above are under `docs/validation/video-financial-model`. Missing Cargo, compilation/setup errors, and early replay-harness errors are not counted as successful RED phases. The optional native-rename-save experiment was reverted; its superseded logs are explicitly identified in `validation-core-evidence.json`.

Small refactors consolidated fill cancellation/target updates and validation helpers; the focused tests and final regression suite cover their resulting behavior.

## Evidence boundaries

- The source template requires an email submission. No email was submitted; `video-start.xlsx` is reconstructed from the visible initial cells and formulas. The initial sheet name and some initial label styling are inferred. Starting label styles were chosen to match the legible final state.
- The recording does not expose physical keyboard events or the mechanism producing the Input sheet's initial two-decimal percentage format. The replay explicitly establishes that format before copying the first zero-percent cell. Both explicit percent input and editing an inherited percent format have separate tests.
- The exact light-blue palette RGB was not available from the inspected frames. The replay uses `#b2dff3`; yellow is `#ffff00`. Font metadata matches Arial 10/9, but font substitution and Qt/Excel chrome prevent a claim of pixel-identical rendering. The source's accounting-style dollar spacing is represented by compact currency in the reconstructed fixture.
- The source's legible final D:K displays are asserted directly. Further months are verified against the visible formulas and independent arithmetic; they were not all legible in the final video crop.
- These are range-backed lists. Arbitrary validation types, inline lists, and all Excel validation semantics are outside this acceptance. Existing conservative guards still refuse structural edits affecting these lists; imported-package validation rename/save restrictions remain in place.
- Microsoft Excel itself was not run. Native save/reopen and independent OOXML inspection passed; Excel execution/interoperability beyond that evidence is unverified.
- Tutorial browser tabs were closed after the user's cleanup request. The replay runs offscreen and does not start a browser.
