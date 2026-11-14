# Native Format Cells and Clear Formats

Implemented in `src/worksheet_cell_format.cpp` and `include/tigersheets/worksheet_cell_format.hpp`; covered by `tests/worksheet_cell_format_tests.cpp`. The grid reset fix is in `src/grid.cpp`.

The private runner uses the project Qt include paths and existing optimized native libraries, writes only `/tmp/tigersheets-cell-format-tests`, and runs with `QT_QPA_PLATFORM=offscreen`. Each passing log records the complete compiler/linker invocation. Shared Ninja ownership remains with the parent task.

Observed cycles:

- `format-direct-red.log`: new public formatting header absent. `format-direct-green.log`: selected rectangular styles preserve text, formulas, booleans, errors, notes and mixed existing styles in one Undo transaction.
- `clear-formats-red.log`: new Clear Formats entry point absent. `clear-formats-green.log`: selected formatting, imported style identity and wrap are cleared while notes and values remain; one Undo restores the source exactly.
- `format-dialog-red.log`: formatting without arguments returns false instead of opening a dialog. `format-dialog-green.log`: real native Number, Alignment, Font and Fill tabs expose current-cell defaults and accessible controls; accepted changes preserve unmodified mixed styles; Escape discards drafts.
- `format-validation-red.log`: unsupported style properties are accepted. `format-validation-green.log`: malformed and unsupported properties reject atomically; discontinuous and over-limit selections refuse without allocating or editing.
- `format-unchanged-red.log`: an empty formatting change creates blank styles/history. `format-unchanged-green.log`: empty changes and unchanged dialog acceptance preserve the workbook and history.
- `clear-unformatted-red.log`: clearing an unformatted range creates blank cells/history. `clear-unformatted-green.log`: no cells/history are allocated for an unchanged clear.
- `format-clear-colors-red.log`: empty fill/font-color reset sentinels return invalid QColor brushes wrapped in valid QVariants. `format-clear-colors-green.log`: the directly compiled grid returns default roles; unrelated styles/notes/values survive and Undo restores the original colors.
- `format-left-alignment-red.log`: explicit Left alignment still renders numeric cells with the default right alignment. `format-left-alignment-green.log`: a single grid branch honors explicit Left for numeric literals and calculated numeric formulas; the full formatting suite passes with directly compiled current grid source.

No live desktop display or Microsoft Excel execution is claimed by these offscreen tests. Native OOXML reset serialization is covered separately by the layout/persistence task.
