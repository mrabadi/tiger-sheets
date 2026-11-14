# Formula workspace regression evidence

`tests/formula_workspace_tests.cpp` exercises the actual MainWindow, native
formula bar, worksheet model and cell delegate using Qt Test keyboard and
mouse events. It does not replace the app's calculation or completion code.

The independent agent run verified two intended RED behaviors before the
corresponding integration fixes:

- F2, `=SUM`, Tab committed incomplete text to the canonical workbook.
- After `set_metadata`, `=IF(1,disco` followed by Tab failed to insert the
  defined name `DiscountRate` without a parenthesis.

Exact observed commands, exit codes, failure lines and the final GREEN
summary are preserved in
`validation/ux-rework/formula-workspace-agent-evidence.log`. These are clearly
labeled **execution transcript excerpts** because the original tool outputs
were not redirected to raw files. The root artifact named
`tigersheets-ux-workbook-context-red.log` ran a stale binary and passed; it is
excluded as RED evidence.

After the app fixes, the independently rebuilt test exited 0 and passed all
seven selectable scenarios:

- `cell_completion`: Tab retains the active cell/editor without a transaction.
- `workbook_context`: defined names, LAMBDAs and enabled VBA functions reach
  bar/cell completion; disabling VBA removes stale suggestions.
- `bar_keyboard` and `bar_mouse`: keyboard selection and real double-click
  insert pending text without committing it or stealing focus.
- `nested_hints`: bar/cell argument help handles nested functions, strings
  and array separators while pending text stays outside the workbook.
- `escape`: first Escape dismisses the list; second Escape cancels the entry.
- `unicode_f4`: bar/cell F4 cycles both range endpoints while preserving the
  Unicode sheet qualifier, apostrophes, case, spacing and cursor suffix.

Compilation used `-Wall -Wextra -Wpedantic` and produced no warnings.
Temporary progression diagnostics were removed before the final GREEN run.
Root CMake runs the complete suite through `formula_workspace_tests`; an
optional scenario name selects one focused case. These offscreen interaction
checks do not establish all Excel behavior or replace desktop visual review.
