# Worksheet context-menu TDD evidence

Version `0.3.0~dev.2`, Ubuntu, 2026-11-12. The requested personal TDD skill was used for implementation. Focused logs retain actual commands or named build/run steps and observed failures; unrelated compile/setup failures are not counted as RED evidence.

Primary implementation files are `src/worksheet_context_menu.cpp`, `worksheet_edit_actions.cpp`, `worksheet_cell_format.cpp`, `worksheet_view.cpp`, `worksheet_layout.cpp`, `grid.cpp`, `session.cpp`, `references.cpp`, `ooxml.cpp` and their headers. `main_window.cpp` connects the commands, frozen row and shortcuts; CMake, the package gate, application version and package verifier identify the development installer. New worksheet tests exercise the public behavior.

- `menu-red.log` → `menu-green.log`: missing cell menu, then real native menu actions.
- `keyboard-red.log` → `keyboard-green.log`: Shift+F10; private menu records also cover the Menu key, selected-range copy, scrolled headers, bounds and stale callbacks. `menu-frozen-red.log` and the shared menu test cover the frozen first row.
- `selection-stale-red.log` → `selection-stale-green.log`: a retained action cannot apply to another selection. The private invalidation cycle covers workbook-context replacement without relying on revision alone.
- `rows-insert-*`, `references-*`, `print-area-guard-*`, `indirect-report-stale-*`: native structural edits, parsed references, overflow/refusal, names, report freshness and undo.
- [Layout cycles](layout.md): native merges, dimensions, package preservation, type/style safety and hidden-row calculation/report invalidation.
- `layout-ui-red.log` → `layout-ui-green.log`, `dimensions-ui-*`, `small-dimension-*`: actual grid spans, units, visibility and reset/undo behavior, including small positive dimensions.
- `format-red.log` → `format-green.log`; [cell-format cycles](cell-format.md): real dialog acceptance/Escape, typed values, mixed formatting, no-op behavior and default brushes. The explicit Left alignment regression is retained separately.
- `structure-ui-red.log` → `structure-ui-green.log`, `merge-ui-*`, `dimension-action-*`: public desktop commands, direction/count, values, visible changes and one-step Undo. Merge warning cancellation and partial-span refusal are additional boundary checks.
- `layout-csv-red.log` → `layout-csv-green.log`: CSV exports cannot claim complete preservation of workbook layout.
- `column-width-ui-red.log` → `column-width-ui-green.log`, `column-width-{example,zero,validation}-*`, `column-width-native-cycles-green.log`: independent Microsoft width expectations, integration and two native save/reopen cycles.
- `wrap-*`: actual native delegate sizing and painting; explicit per-cell wrapping works with global wrapping disabled.
- `merged-*`: clipboard/sort refusal, bounded rectangle preflight, and atomic protection against non-anchor merged-cell writes. Safe anchor edits, Values/Formats paste and imported original values are retained.
- `format-shortcut-red.log` and the final shared shortcut test: actual Ctrl+1 dialog opening and cancellation without workbook changes.

`format-before-consistent-rebuild.log` records an in-flight check linked to an older grid archive while the Left fix was being compiled. It is not a product qualification result; the final consistent build and full suite supersede it.

`parallel-ctest-before-final-reference-rebuild.*` retains the initial complete run: two structural tests used an older reference archive, and the reporting UI-deadline test exceeded its dispatch budget while competing with another test process. A direct current-source build and rebuilt shared reference tests pass. The reporting test passes in isolation (`reporting-publication-isolated.log`). Its CTest entry now uses `RUN_SERIAL`; measurement thresholds and assertions are unchanged. `final-reference-build.log` and the final full CTest results record the correction.

The final build, CTest XML/log, source manifest and package audit are recorded alongside these focused cycles. Screenshots show actual Qt menus/dialogs. Validation uses Qt offscreen unless explicitly stated; it does not qualify a real X11/Wayland session, screen-reader workflow, clean-machine installation or Microsoft Excel interoperability.
