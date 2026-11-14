# Clipboard and worksheet interactions

Copy and Cut display an animated dashed border around their source range. The border stays on the source while navigating or repeatedly pasting. It pauses when the source is offscreen or on another sheet, resumes on return, and follows scrolling, resizing and frozen rows. Escape, clipboard replacement, workbook replacement and new edits cancel the animation. Calculation refresh does not cancel it. Failed Cut/Paste preserves the pending source; successful Cut/Paste consumes it.

Right-click a sheet tab for Rename… or Delete. Double-click an actual tab to rename it; keyboard context-menu requests use the active tab. Rename updates references and reports invalid names. Delete confirms with Cancel as the default, chooses a surviving tab, and is disabled for the last worksheet. Undo restores deleted sheet contents, styles and order.

Selected cells retain their fill, text and font formatting beneath a light tint. A green perimeter encloses the selected range. Whole-row and whole-column selections no longer paint an unrelated cell's border, focus frame or fill handle. Extending a selection invalidates its previous perimeter. Header gestures already produced the correct model selection; the extra cell was a rendering artifact.

The workbook header displays an accessible text badge beside the filename: **Not saved**, **Unsaved changes** or **Saved locally**. Unsaved states use an amber background and the native modified-title indicator. A successful local save clears the indicator; edits, Undo and Redo restore it. Cancelled or failed saves and CSV exports preserve the source workbook's unsaved state. Recovered workbooks explicitly require a new save. Status remains fully visible at the supported minimum window width, including beside long filenames.

Undo uses the existing conservative dirty-state behavior: it remains unsaved until Save, even when values happen to match an earlier saved state. This change does not introduce history savepoints or claim complete Excel parity.

## Executed TDD evidence

- `QT_QPA_PLATFORM=offscreen build-release/worksheet_clipboard_ui_tests copy_animation` first failed because Copy displayed no source border. The transparent viewport overlay, source geometry and looping dash animation made the test pass. Tests advance Qt's animation clock directly and inspect the actual widget geometry and phase, without screenshot files or wall-clock sleeps.
- The same binary's `copy_escape`, `clipboard_replace`, `copy_edit` and `copy_reopen` selectors each reproduced a missing cancellation boundary before controller changes; all passed afterward. The already-working repeated-paste and sheet-switch paths were characterized by `repeat_paste` without inventing a failure.
- `frozen_copy` first reproduced the missing frozen-row border. Propagating source state to the frozen view passed it; a subsequent failing edit-start assertion led to cancellation from the frozen editor factory too. `copy_scroll` verifies true offscreen geometry, animation pause/resume, resizing and editing. Existing Cut tests now also cover the outline, a failed Paste Special and successful move completion.
- `QT_QPA_PLATFORM=offscreen build-release/worksheet_sheet_actions_tests` has six independently reproduced RED→GREEN slices: native Rename menu, Delete menu and confirmation, last-sheet protection, Rename cancellation/validation, blank-tab-strip double-click isolation and keyboard menus. Values, styles, reference rewriting and atomic history are checked through real UI actions.
- `QT_QPA_PLATFORM=offscreen build-release/worksheet_selection_ui_tests` has six independently reproduced RED→GREEN slices: formatted fills/text, full-range perimeter, native row-header drag, native column-header drag, previous-edge invalidation and current-cell artifacts outside selection. The native style options and vector drawing commands are inspected in memory without raster screenshots. Existing entry, fill-handle, wrap and layout tests also passed (5/5 related suites).
- `QT_QPA_PLATFORM=offscreen build-release/workbook_save_state_ui_tests new` first failed because new workbooks had no visible header status. The badge and native modified-title state passed it. `transitions` then failed because editing left the header at Not saved; synchronizing the header on edits and successful saves passed real XLSX save/reopen, no-op edit, Undo and Redo checks. `recovery` reproduced the previous file's Saved locally status after recovering different data; synchronizing recovery passed it. The already-correct failed-write, native Save As cancellation and CSV-export boundaries were characterized without inventing failures.
- A corrupt recovery-database fixture reproduced a failed save changing the header filename while retaining the old native document identity. Deferring the path and filename update until the final recovery flush succeeded passed the regression; retrying native Save still required choosing a file. Template replacement separately reproduced a stale filename from the previously opened workbook; resetting the template name before its edits passed it. All six save-state scenarios and six related suites passed.

No video replay test or screenshot output was added. The build explicitly reuses the existing calculation archive; no Rust or vendored engine source changed.

## Final verification

- `cmake --build build-release -j2` — passed.
- `ctest --test-dir build-release --output-on-failure -j2` — **136/136 passed**, zero failures (37.12 seconds).
- Development revision **0.3.0~dev.4**: actual-DEB version/filename/upgrade-order tests and validator-default tests each failed against dev.3 before the version bump, then passed. The focused Python suites passed 7/7 and 9/9, respectively, and passed again within CTest. The desktop `--version` assertion likewise reproduced dev.3 before the application version update and passed afterward.
- `git diff --check` — passed before packaging.

These checks exercise the Qt UI offscreen and real local workbook persistence. They do not establish complete Excel compatibility, native platform acceptance or a clean-machine installation.

## Installer audit

`cpack --config build-release/CPackDevelopmentConfig.cmake -B build-release` generated `tiger-sheets_0.3.0~dev.4_amd64.deb` (82,788,998 bytes).

`python3 tools/verify_development_package.py build-release/tiger-sheets_0.3.0~dev.4_amd64.deb --expect-version '0.3.0~dev.4' --output build-release/tiger-sheets_0.3.0~dev.4_amd64.deb.validation.json` passed. Checks covered DEB metadata, desktop/MIME/icons, ELF runtime paths and dependencies, Qt platform plugins, the staged app version, worker protocols and a real Solver result (objective 40; changing constant 20). A read-only APT simulation accepted the upgrade from installed dev.3 without removing packages. No system installation was performed.

Package SHA-256: `4e4244da572ef77b63c792863f08b0722ac55571176f80f1c626a31185836dfd`.

The checksum sidecar and raw audit remain beside the ignored local installer. This audit record was appended after packaging, so the packaged copy of this document ends with final test verification. Application binaries were unchanged after their audit.
