# Native chart interaction and editing

The chart widget previously only painted data. This change adds chart selection, editing requests, movement and resizing without allowing the view to change the authoritative workbook itself.

`ChartWidget` exposes selected state, `isInteracting()`, and copied `std::function` callbacks for selection, editing, deletion, and committed geometry. Geometry uses parent coordinates. A host translates a completed gesture into its own workbook transaction; move events only preview the widget's geometry. Escape restores an active gesture's starting geometry without a commit. Escape outside a gesture clears selection. All four corner handles resize with the opposite corner fixed and retain the existing 320 × 230 minimum size. A completed gesture includes the final mouse-release position even when intermediate move events were compressed.

Double-click and F2 request general editing. The context menu exposes Edit chart, Select data, Change chart type, and Delete chart. Selected charts accept shortcut-override events for bare Delete, Backspace, F2 and Escape so an ancestor worksheet's Delete shortcut cannot clear cells while a chart owns focus. Callbacks are copied before execution; tests allow the host to synchronously replace the sender on edit, delete, and geometry commit.

`ChartEditorDialog` clones the supplied specification and returns a separate accepted result through `resultSpecification()`. It preserves chart ID, anchor and unknown properties. Cancel/Escape returns no candidate and does not modify the original. It edits title, type, worksheet and a rectangular A1 data range, including optional dollar signs and lowercase references. Worksheet IDs are retained separately from visible worksheet names. Ranges require headers plus at least one category and series, stay inside Excel's worksheet bounds, and respect the renderer's existing limits of 10,000 data rows and 100 series. Native type choices are line, column, bar, area, scatter, pie and doughnut. An unsupported imported type is displayed explicitly and requires the user to choose a supported type before accepting an edit.

Stable dialog object names are `chartEditor`, `chartTitle`, `chartType`, `chartRange`, `chartSheet`, `chartDialogButtons` and `chartValidationError`. The context menu is `chartContextMenu`; action names are `chart.edit`, `chart.selectData`, `chart.changeType` and `chart.delete`.

## RED → GREEN evidence

Tests were built in the independent `/tmp/tigersheets-chart-ux` CMake harness linked to Qt Widgets and Qt Test. No shared project CMake or MainWindow source was edited for this work.

- `/tmp/tigersheets-chart-selection-red.log`: a real chart click failed to acquire keyboard focus; after adding focus support, the same test passed.
- `/tmp/tigersheets-chart-state-red.log`: the deliberately new `isSelected()` public contract was absent. Selection and idle Escape behavior then passed.
- `/tmp/tigersheets-chart-drag-red.log`: a real Qt mouse-move event produced no relative movement or geometry callback before the drag implementation. After implementation it moved by the pointer delta and committed once. The offscreen platform does not deliver `QTest::mouseMove` as a held-button move, so the test sends a real `QMouseEvent` through QApplication while using QTest for press/release; this repaired harness was rerun against the implementation without gesture handling to verify RED.
- `/tmp/tigersheets-chart-cancel-red.log`: Escape did not restore an in-progress move without committing; the same test passed after cancellation handling.
- `/tmp/tigersheets-chart-resize-red.log`: a corner moved the chart instead of resizing it; corner detection and constrained resize passed the same test.
- `/tmp/tigersheets-chart-keyboard-red.log`: selected-chart Delete did not request chart deletion while an ancestor had a `WidgetWithChildrenShortcut`; shortcut override plus chart key handling passed.
- `/tmp/tigersheets-chart-edit-actions-red.log`: double-click did not request editing; the completed double-click, F2 and keyboard-driven context-menu tests passed.
- `/tmp/tigersheets-chart-release-red.log`: a fast drag with only press/release did not move. Applying the final release position through the same gesture helper passed.
- `/tmp/tigersheets-chart-editor-cancel-red.log`: the deliberately new dialog constructor/result contract was not implemented. Native cancel behavior then passed.
- `/tmp/tigersheets-chart-editor-edit-red.log`: native type/worksheet/range fields were absent. Accepting edited fields then returned a separate candidate with the original ID, anchor and unknown extension.
- `/tmp/tigersheets-chart-editor-validation-red.log`: `A1:A4` was accepted without a data series. Validation then kept invalid/reversed/oversized/out-of-bounds/malformed ranges editable with an error and refused implicit conversion of an unsupported native chart type.

Each focused test was run before its production behavior was added. Initial test setup was repaired before retaining meaningful RED evidence. Refactoring extracted a shared gesture update helper so moves and final releases use the same coordinate calculation.

## Verification and boundaries

Normal focused tests also cover all four resize handles, minimum dimensions, interaction-state lifetime, synchronous sender replacement, legal absolute lowercase references at XFD1048576, a required worksheet, and existing live-series, horizontal-bar and million-value axis-label rendering regressions. The Qt offscreen plugin emits benign warnings for menu raising, keyboard grabs and size-hint propagation.

Canonical transactions, undo/redo, preview policy, chart reconciliation by ID, grid-to-anchor coordinate conversion, native offset persistence and installed MainWindow integration belong to the host integration and are verified separately. This change does not broaden chart rendering or OOXML type support.

Retained raw evidence is in `docs/validation/chart-editing-red.log` and `docs/validation/chart-editing-green.log`. The latter records exit code 0 for both focused normal tests and both focused AddressSanitizer/UndefinedBehaviorSanitizer tests. The independent sanitizer harness used `-fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic`; its build log contained no compiler warnings. Leak detection was disabled (`ASAN_OPTIONS=detect_leaks=0`) for the existing environment limitation. Tests ran with `QT_QPA_PLATFORM=offscreen`. Parent-run whole-application/integration checks are outside these focused results.
