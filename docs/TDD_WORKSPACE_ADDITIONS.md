# Assembled application: additional observed TDD cycles

These records cover specific behaviors; the full release contract is still unfulfilled.

- A native formula-entry test failed because model resets lost the active cell. Loading a snapshot now emits data changes while retaining the active index. Formula-bar editing and Fill Down pass through the real engine and native file writer.
- The F4 UI regression failed while a formula-bar reference was being edited. The event filter now cycles the reference under the cursor, with the four lock modes and quoted-text exclusion covered separately.
- Structured column labels such as `Sales[A1]` were incorrectly shifted by copying. `references_tests` observed that failure; bracketed labels now stay intact in copy and F4 routing.
- The Developer command test failed because the visible command had no handler. It now opens the real source editor/worker. Opening another workbook destroys that editor before replacing its workbook identity.
- A fake-process Codex test failed to compile because workbook reset was absent. The reset now disconnects the old process and clears its thread identity. Reconnecting for another workbook starts a new thread rather than resuming the previous workbook conversation.
- Recovery originally stored empty source bytes from the desktop. `workspace_tests` observed the byte mismatch; checkpoints now retain the original file alongside the edited snapshot. A subsequent restore test failed because the desktop had no recovery action; File → Recover now restores the original package and unsaved edits after source deletion, requiring a new save path.
- `scheduler_tests` first failed because the scheduler API was absent. It now runs calculations on a dedicated thread. A deliberately blocked first calculation is superseded by a second revision, and only the latter reaches the UI. Cancelled generations are suppressed. An actual workbook calculation also passes. Cancellation discards output; it does not interrupt an executing IronCalc evaluator, and shutdown waits for that evaluator.
- `chart_tests` failed a rendered pixel check because a bar chart was drawn as columns. Native bar charts now draw horizontally and pass that rendering check.
- The sheet-rename regression failed because local formulas and global named references retained the old name. Local direct references now follow the renamed sheet, including escaped quotes; literal strings and external-workbook qualifiers remain unchanged. This does not establish complete structural-reference conformance.
- The worksheet-name validation regression accepted an ASCII newline. Names now reject ASCII control characters and DEL.
- `workspace_tests` failed because Ctrl+End used Qt's worksheet limit rather than Excel's used range. Native routing now handles Ctrl+Home/End and Ctrl+arrows over populated regions, with context kept separate from line editing.
- Ubuntu packaging tests failed for missing launcher identities and release gate. The desktop/MIME files are now present and validated, and CPack refuses an uncovered compatibility report.

Commands and final assembled test results are recorded in `docs/RELEASE_VALIDATION.md`. The performance benchmark produces measurements rather than treating unachieved budgets as passing tests. Full performance, native VBA authoring, and the remaining catalog entries still block release.

## Final integration corrections

Peer review reproduced five desktop failures against the assembled libraries; the original teardown trace is retained in `docs/validation/desktop-teardown-red-trace.txt`. New workspace regressions observed missing recovery after Accept, omission of an active cell edit during Save, preview isolation lost through View commands, and an asynchronous missing-chart-range exception. The corrected paths commit pending edits, checkpoint accepted proposals, preserve candidate display state, and diagnose display errors. Child widgets are destroyed while their authoritative session still exists.

The local workbook link UI regression initially failed because Refresh had no command handler. The desktop now prepares cached links without reading files, reads local sources only on explicit Refresh, and retains the loaded snapshots for ordinary recalculation. The same test deletes the source file after refresh and confirms subsequent formula edits reuse the loaded snapshot.

The 1904 display regression initially showed serial zero using the 1900 base. Numeric date display now uses the workbook's native 1904 base. Calculation of 1904 workbooks remains explicitly unsupported and reports that status instead of silently evaluating with the wrong date system.

The CSV desktop regression initially wrote formula source. Export now uses a current typed calculation through `csvValuesSnapshot`, preserves the XLSX identity and unsaved recovery, and does not mark the workbook clean merely because values were exported. Saving an active formula-bar edit without Return is also covered.

Catalog identity validation observed two duplicated shortcut IDs (226 rows but 224 unique IDs). Section suffixes now distinguish those source rows without changing their keys or scope; the catalog test passes. A release-gate shape regression accepted Boolean `test` and `cases` values as evidence. Evidence now requires a nonempty test name and a nonempty list of named executed cases; the same tests pass.


## Financial chart label regression

The installed application's screenshot exposed clipped value-axis labels for million-value financial charts. A rendered glyph-coverage regression in `chart_tests` first failed with the fixed 52-pixel axis (`163 / 190` dark glyph pixels, less than the 90% minimum). The tolerance allows antialiasing differences; the expected image renders the complete independent label `1.08e+06`. An initial 98% threshold was too strict even when the full label fitted (`179 / 190`), so the test was corrected and the fixed-width failure reproduced again before accepting the implementation.

The renderer now measures all displayed tick labels in the actual font and sizes the value-axis margin accordingly, sharing the existing numerical bounds between layout and painting. Horizontal bar behavior is retained. The focused test passed after this change. Raw RED and build output are retained in `docs/validation/chart-label-red.log` and `chart-label-green-build.log`. This validates the observed clipping defect; it does not establish full chart-layout conformance.
