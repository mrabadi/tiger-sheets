# Cell formatting and editing regressions

Use Home → Format… or Ctrl+1 to configure display formats. Number and Percentage offer decimal places. Number also offers grouping and negative-number styles. Currency offers a symbol, decimal places and minus-sign or parentheses negatives; Accounting uses parentheses and a zero dash. Date offers display types while retaining the exact format inferred from the entered numeric date. Text and custom format codes remain available. The Sample previews the display without changing the stored value. Cancel and unchanged acceptance preserve history.

The Home number-format selector follows the selected cell, formatting changes, Undo/Redo, sheet switching and workbook reopening. In-cell editors fit within normal, narrow, scrolled and merged cells. Selecting a formula outlines its local references; selection changes and formula replacement invalidate the previous outlines, and an active editing draft takes precedence.

## Executed RED → GREEN cycles

- `theme_tests`: a compact cell was 111×23 while its editor was 101×24. Removing the inherited line-edit chrome and minimum dimensions kept the editor inside the cell.
- `worksheet_entry_tests`: a single click on a formula did not outline its referenced range. Using the selected formula when no draft exists, then invalidating the full viewport on selection/formula changes, passed the outline and repaint regressions.
- `grid_tests`: the EOMONTH result displayed serial 45961 instead of 10/31/2025. Date-producing formulas without an explicit format now receive a date format. Typed numeric dates store canonical serials while retaining input order, separators and component widths. Date editing preserves values, including unchanged edits, two-digit-year boundaries, fractional times and the 1900/1904 date-system boundaries.
- `number_format_ribbon_tests`: selection retained the previous format category; the Custom choice used a separate editor and cancellation retained the wrong category. The selector now synchronizes from the cell and opens the native Format Cells dialog for Custom. A visible Format… button opens the same options.
- `number_format_ribbon_tests date_style`: changing a date to General left the date text in the formula bar. Synchronizing clean editor text with model changes passed the regression, including Undo, unchanged Enter, preservation of drafts and cursor selection. Enter normally moves down; the test reselects the original cell before checking Undo.
- `worksheet_cell_format_tests`: Number, Currency, Accounting, Percentage and Date options were absent. Separate failing tests preceded each control group. Live samples, value preservation, Cancel, unchanged acceptance and atomic Undo are covered.
- `worksheet_cell_format_tests general_text_custom`: General/Text categories were missing. Those choices now coexist with editable custom codes, while an unchanged Custom dialog preserves exact codes and mixed styles.
- `grid_tests text_entry`: entering `00123` in a Text-formatted cell lost its literal form. New entries such as `00123`, `10%` and `=1+2` now stay text; existing numbers merely formatted as Text retain their numeric value. Escapes, blanks, style preservation and one-step Undo are covered.

`worksheet_fill_handle_tests formats` and `inferred_date` passed on their first run as characterization tests. Actual horizontal and vertical drag gestures already copied the complete source style over conflicting populated destinations. Calculated displays verified date sequences, percentage precision, grouped decimals and parenthesized currency, plus shifted formulas, preview immutability and one-step Undo. No fill implementation change was needed; the missing inferred date style was corrected at entry.

The video replay target and source have been removed. The original walkthrough remains historical evidence. These regressions do not write screenshot files or use the video replay.

## Focused verification

Examples of the direct commands used for verified failing-before/fixed-after cycles:

```sh
QT_QPA_PLATFORM=offscreen build-release/worksheet_cell_format_tests number_options
QT_QPA_PLATFORM=offscreen build-release/number_format_ribbon_tests date_style
QT_QPA_PLATFORM=offscreen build-release/grid_tests text_entry
```

The first failures respectively reported missing Number controls, a stale date in the formula bar after choosing General, and lost literal Text input. All three commands passed after their corresponding minimal fixes. The remaining format-option slices used the same dialog test binary with the selectors `currency_options`, `accounting_options`, `percentage_options`, `date_options` and `general_text_custom`.

```sh
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure \
  -R '^(grid_tests|theme_tests|worksheet_entry_tests|worksheet_cell_format_tests|number_format_ribbon_tests|worksheet_fill_handle_tests|worksheet_percentage_entry_tests)$'
```

Final verification: `cmake --build build-release -j2` passed; `ctest --test-dir build-release --output-on-failure -j2` passed **133/133**, with zero failures (28.86 seconds). CTest's registered-test inventory contains the new ribbon test and no video replay target. `git diff --check` passed.

The calculation archive is reused explicitly for this C++-only development build; no Rust or vendored engine source changed. These tests qualify the described interactions, not complete Excel compatibility or Microsoft Excel interoperability.
