# Assembled desktop review observations

These failures were observed in a read-only review against the assembled native desktop libraries. Only temporary reproducer files under `/tmp` were created; no application or shared test sources were changed by that review. The original evidence below is retained. Subsequent root fixes are now present in source, and `docs/validation/ctest.log` records all 28 optimized native test targets passing; its timestamp identifies the final executed run. The packaging dependency correction recorded at the end has its own focused RED/GREEN evidence and was included in root regeneration and temporary-prefix staging.

## Saving an active editor

Reproducer: `/tmp/tiger-ui-review.cpp`, compiled with the workspace test's include/define flags and linked to the same assembled libraries as `workspace_tests`, using Qt's offscreen platform.

1. Show a `MainWindow`, edit A1 in its `QTableView`, and set the active editor text to `77` without pressing Return.
2. Call `saveWorkbook` while the editor retains focus.
3. Read the saved package through `readWorkbook`.

Observed output: `pending-cell save-success=1 file-cells=[] editor=77`. The saved snapshot omitted the pending edit. The save path must commit the active cell/formula editor before taking the snapshot and reporting a successful save.

Resolution: `MainWindow::commitEditor` now commits the cell editor and modified formula bar before saving. It also runs before the close dirty check and Open/template routing. The passing `workspace_tests` checks that an active cell containing `77` and a formula bar containing `=6*7`, each without Return, are saved correctly.

## Accepted Codex preview recovery

The same reproducer first committed and saved A1, then created a revision-checked `WorkbookSession` preview changing A1 to `99` and ran `executeCommand("preview.accept")`.

Observed output: `accepted=1 recovery-count=0 cell=[{"col":1,"input":"99","row":1}]`. Accepting the preview changed the canonical workbook but bypassed the ordinary `WorksheetModel::onChanged` recovery checkpoint. The accepted edit must trigger the same dirty/status/checkpoint path as other canonical edits.

Resolution: the accepted-preview path now invokes `model->onChanged` after refreshing the accepted snapshot. The passing `workspace_tests` verifies that accepting the proposal creates a recovery checkpoint.

## Active-editor teardown

Destroying that shown window with a pending editor aborted with `json.exception.type_error.305`. A temporary `__cxa_throw` tracer recorded this call chain:

```text
MainWindow::~MainWindow
QWidget::~QWidget / hide / clear focus
QStyledItemDelegate::setModelData
WorksheetModel::setData
WorksheetModel::apply
session_.snapshot()["revision"]
```

The `State` member containing `WorkbookSession` had already been destroyed before Qt's base widget teardown committed the editor. Trace: `/tmp/tiger-ui-review-full-trace.txt`; tracer source: `/tmp/tiger-throw-trace.cpp`. Editor and central-widget teardown must occur while `State` remains alive, with root callbacks disconnected as appropriate.

Resolution: the destructor now stops and destroys the scheduler, destroys workbook macro dialogs, disconnects child-to-window callbacks, and destroys the central widget and worksheet model while `State` is still alive. The optimized workspace lifecycle tests passed in the recorded 28-test run. The exact original temporary reproducer was not rerun during this final read-only audit; its original failure remains preserved in `docs/validation/desktop-teardown-red-trace.txt`.

## Worksheet navigation during preview

Reproducer: `/tmp/tiger-ui-preview-review.cpp`, linked in the same way. It invoked the actual host `workbook_v1_preview` callback, then changed the selected worksheet tab.

Observed output:

```text
preview-editable-before=0
preview-editable-after-sheet-navigation=1
```

The tab handler called `setSheetId`, whose `refresh` loaded the canonical workbook and cleared preview/read-only state. Navigation and other refresh paths must keep displaying the candidate with editing disabled until acceptance or discard.

Resolution: `WorksheetModel` retains the candidate snapshot and keeps `setSheetId` on it while previewing. The formula-view command also preserves the preview. Passing `grid_tests` checks switching to the second candidate worksheet and back, refused edits, unchanged canonical data, and restoration after discard. Passing `preview_ui_tests` checks the actual host preview followed by a view command.

## Invalid chart metadata reaching an asynchronous callback

The second reproducer applied `set_metadata` for `charts` with a valid range and `anchor.row` set to the string `bad`. The operation succeeded. When the scheduled calculation delivered its result, `displayCalculation` attempted to use that anchor as a number.

Observed output:

```text
bad-anchor-accepted=1
async-exception-escaped=[json.exception.type_error.302] type must be number, but is string
```

The exception escaped `app.processEvents`; a normal event loop had no application-level handler for it. Metadata validation should reject invalid candidates before commit, and asynchronous display callbacks should diagnose failures without throwing through Qt event dispatch.

Resolution: session construction, transactions, and previews now validate chart range/anchor fields and relevant VBA/style types. Passing `session_tests` verifies malformed anchors roll back the entire transaction and cannot become acceptable previews or imported sessions. `displayCalculation` now catches display errors and records diagnostics. Passing `preview_ui_tests` checks the diagnostic path for preserved incomplete legacy chart metadata without an exception escaping the event loop.

No additional scheduler ownership fault was found in this review: generation tokens reject superseded results, delivery uses the scheduler as its QObject context, and the destructor waits for the worker before destroying its state. CSV formula export and broader compatibility gaps were outside this review's conclusions.

## Final install and packaging audit

The audit did not run a build or CPack. It inspected the root-generated stage recorded in `docs/validation/install-stage-path.txt` and the existing build/test/install/gate logs.

- `desktop-file-validate` passed. MIME and QRC XML parsed, and all eight QRC aliases resolved to square RGBA icons at the expected sizes. The existing Ubuntu MIME database uses the same XLSM type spelling as the desktop entry.
- The staged executable and worker are present with executable permissions at `usr/bin/tiger-sheets` and `usr/libexec/tigersheets/tigersheets-vba-worker`; the worker lookup in source matches that layout. Desktop, MIME, icon, solver runtime, source and license paths are present.
- The staged executable has `RUNPATH $ORIGIN/../lib/tiger-sheets`; the staged pagmo library has `RUNPATH $ORIGIN`. `ldd` found no missing libraries for the executable, worker, or pagmo on this development host. This is not a clean-host dependency qualification.
- The five retained solver archives match every SHA-256 in the pinned manifest. The install rules preserve solver sources/licenses and the 72-package Rust vendor source tree. Project distribution licensing remains explicitly unfinished in the README.
- The audit found that dynamically loaded Qt platform plugins were absent from the explicit Debian dependency list. The focused packaging regression failed before the correction, then all three packaging tests passed after declaring `qt6-qpa-plugins, qt6-wayland`. See `TDD_PACKAGING.md` for verified Ubuntu names and exact evidence. Root regeneration/staging is separate from this source-level check.

The root's recorded CPack run was correctly blocked by the compatibility gate: 215 required function entries, 190 shortcut entries, and 21 feature groups lacked passing evidence. No qualified release package resulted. Remaining material release risks are clean install/upgrade and X11/Wayland qualification, complete dependency/distribution licensing review, and the compatibility gaps retained in `RELEASE_VALIDATION.md`. This audit found no additional obvious launcher, icon, worker deployment, or solver runtime path failure.
