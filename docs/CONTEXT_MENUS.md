# Worksheet right-click menus

TigerSheets `0.3.0~dev.2` adds compact menus for worksheet cells, row numbers, column letters, and the frozen top row. Right-click within an existing selection to keep it; right-click elsewhere to select that cell or header. **Shift+F10** or the **Menu key** opens the cell menu from the keyboard. Escape closes it.

The cell menu provides Cut, Copy, Paste, Paste Special (values, formulas, formats and transpose), insertion above/below or left/right, row/column deletion, Clear Contents, Clear Formats, merge/unmerge, sorting, notes and Format Cells. Row and column menus add height/width, Hide and Unhide. Select several contiguous rows or columns to insert, delete or resize that count. Select visible headers on both sides of hidden rows/columns before choosing Unhide.

Merge Cells preserves the upper-left value. TigerSheets asks before removing other populated values; declining changes nothing. Merge & Center also centers the retained value. Unmerge keeps the retained value in the upper-left cell. These changes, including removed values, are restored together by Undo.

Format Cells has Number, Alignment, Font and Fill tabs. It preserves mixed formatting for controls you leave unchanged. Clear Formats retains notes and values. Wrap Text uses the available row height; Row Height lets you make additional lines visible. Column Width accepts character units; XLSX uses the native SpreadsheetML width representation.

Structural edits rebase ordinary, absolute, mixed, whole-row/column, cross-sheet references and supported defined names. Saved merge regions, dimensions and visibility move with inserted/deleted rows/columns. XLSX stores these properties natively; CSV export explains that XLSX is needed to retain them.

## Development limits

This implements common Excel-style context commands, not the complete Excel menu contract. Unsafe structural edits involving unsupported imported objects or dependent reports are refused before mutation. Existing source packages remain preservable.

- Formatting and merging operate on one rectangular selection of at most 100,000 cells. Each sheet supports at most 10,000 merged rectangles.
- Dense clipboard transfer is limited to one million cells. Oversized Copy/Cut entries are disabled with an explanation; a complete Excel column exceeds this bound. Clear Contents uses the sparse workbook and remains available for complete rows/columns.
- One resize/hide/unhide action covers at most 10,000 selected rows/columns.
- Moving merged cells, copying their merge geometry, and sorting through merges are not supported in this build. Unmerge first. Values/Formats paste is allowed where it cannot overwrite a merge incorrectly; other unsupported intersections are refused with an explanation.
- Unsupported imported table/chart/pivot/link structures, native scoped names, special cell representations or formula forms can prevent row/column edits. TigerSheets reports the reason instead of changing their meaning.

Save as XLSX/XLSM to retain workbook layout. Complete Excel compatibility and actual Microsoft Excel reopen behavior remain unverified.
