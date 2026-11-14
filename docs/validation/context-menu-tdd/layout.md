# Worksheet layout and native persistence TDD evidence

The native layout contract is `mergedRanges: [{row1,col1,row2,col2}]`, `rowLayout: [{index,size?,hidden?}]`, and `columnLayout` with the same sparse shape. Sizes use native Excel units: row points and column character widths. `set_sheet_layout` replaces the supplied collection. It never clears cell contents; the desktop merge confirmation owns that separate, atomic action. Hidden row state also reaches the calculation engine through `hiddenRows`.

Focused tests:

- `worksheet_layout_tests`: merge/dimensions, atomic overlap and type/bounds/unknown-property refusals, generated output and source ownership, table scope inheritance, hidden-row calculation state, merge expansion/shrink/removal, dimension and hidden-index rebasing, and insertion overflow.
- `worksheet_layout_ooxml_tests`: native mergeCells, row ht/hidden, column width/hidden roundtrip and removal; preserved-package structural preflight, save-time repeat preflight, cross-sheet opaque formula protection, imported table merge ownership, and conservative unsupported native value guards.
- `worksheet_structural_style_tests`: deleting a row must not leave the overwritten coordinate's native bold style on an unformatted cell moved into it; native No Fill, Automatic font color and General alignment resets preserve independent bold and wrapping settings.
- `worksheet_layout_pivot_tests`: a real native pivot over a formula on another worksheet becomes stale when an indirectly referenced SUBTOTAL input is hidden. Actual recalculation changes 30 to 10; undo/redo restores row visibility and report freshness together. A height-only edit on the indirectly referenced sheet does not invalidate the report.

RED/GREEN records are retained beside this file. The initial new API compilation failed because the public layout header was absent. Subsequent RED runs reported the intended behavior message, then the same focused program passed after the minimal repair. Tests used direct `/usr/bin/c++ -std=c++20 -Iinclude` compilation into `/tmp`, and native persistence tests linked the pre-existing optimized dependency archives. No concurrent shared Ninja invocation was used.

The following individual cycles were observed:

| Cycle | RED observation | GREEN result |
| --- | --- | --- |
| Basic layout | Public layout header absent | Worksheet layout tests passed |
| Overlap | overlapping merges reject atomically | Worksheet layout tests passed |
| Validation | invalid or unknown layout properties reject atomically | Worksheet layout tests passed |
| Ownership | merge cannot destroy report/table source semantics | Worksheet layout tests passed |
| Calculation | hidden rows are passed through to SUBTOTAL calculation | Worksheet layout tests passed |
| Structural rebase | inserting inside merged range expands it | Worksheet layout tests passed |
| Scoped table range | table-owned range inherits its worksheet identity | Worksheet layout tests passed |
| Native SpreadsheetML | merged regions persist in native SpreadsheetML | Worksheet layout OOXML tests passed |
| Structural preflight | Public preflight function absent | Worksheet layout OOXML tests passed |
| Cross-sheet formulas | structural preflight protects opaque references from other worksheets too | Worksheet layout OOXML tests passed |
| Native cell guard | structural preflight refuses cells with unsupported native value or formatting semantics | Worksheet layout OOXML tests passed |
| Shared rich text | shared-string rich text cannot be flattened by structural edits | Worksheet layout OOXML tests passed |
| Moved styles | moving an unformatted cell does not inherit overwritten native formatting | Worksheet structural style tests passed |
| Indirect hidden-row cache | hiding an indirect SUBTOTAL input marks every native pivot stale | Worksheet layout pivot tests passed |
| Native style resets | Empty fill reset was refused by the RGB export validator | Worksheet structural style tests passed |

The preservation preflight deliberately rejects row/column structural edits when native tables, charts, pivots, external-reference parts, scoped names, panes, unmodeled grouping, validation/conditional formulas, or affected native values cannot safely be rebased. Plain error/ISO-date/rich text cells and cell metadata are preserved during unrelated layout edits, but moving them is currently refused. This is not full Excel structural compatibility. Merges, dimension sizes and hidden flags are written into native SpreadsheetML; supplemental TigerSheets metadata is not required to read them.
