# Native formula editing

`FormulaLineEdit` is a reusable native `QLineEdit` subclass for the formula bar
and cell delegates. It edits text only; it does not calculate, commit cells or
mutate the workbook. The caller supplies the frozen function catalog and the
actual calculation engine's availability catalog once:

```cpp
auto* edit = new tigersheets::FormulaLineEdit(parent);
edit->setFunctionCatalog(frozenCatalog, calculationEngine.functions());
edit->setWorkbookCompletions(definedNames, workbookFunctions);
```

Built-in completion offers only canonical names present in both catalogs.
Workbook-derived names and functions come separately from the caller; the
caller identifies LAMBDA names and enabled VBA functions. Engine availability
is not presented as a claim of complete Excel conformance. The editor has no
`Q_OBJECT`/moc requirement; use `dynamic_cast<FormulaLineEdit*>` when needed.
The production module links Qt6 Widgets and uses nlohmann JSON headers.

Cell delegates must route key presses through the editor before Qt's default
Tab commit handling:

```cpp
if (auto* edit = dynamic_cast<tigersheets::FormulaLineEdit*>(object);
    edit && event->type() == QEvent::KeyPress &&
    edit->handleFormulaKey(static_cast<QKeyEvent*>(event)))
    return true;
return QStyledItemDelegate::eventFilter(object, event);
```

Normal inherited text/cursor APIs and signals remain available. A completion
uses `QLineEdit::insert`, preserves the suffix and undo semantics, sets the
modified flag, and does not emit Return or finish editing. F4 passes through
for the application's existing reference cycling. Return retains inherited
behavior; Tab and double-click accept completion.

## Behavior and local help

- Case-insensitive function prefixes show an accessible native list. Up/Down,
  Home/End and PageUp/PageDown navigate; Tab inserts the canonical name and an
  opening parenthesis. Existing parentheses and the formula suffix survive.
- Completion works inside existing nested expressions. Quoted strings,
  escaped quotes, quoted sheet names, bracket contents, and A1/mixed-absolute
  or whole-column reference tokens suppress function suggestions.
- Argument help follows the innermost available function at the cursor,
  highlights its zero-based current argument, ignores separators in strings
  and array constants, and returns to the outer call when a nested call closes.
- Escape dismisses assistance without changing text. Cursor motion does not
  reopen it; a subsequent edit or explicit Alt+Down enables fresh suggestions.
  Alt+Down also toggles an open list off.
- Losing focus or making the editor read-only closes assistance. A read-only
  editor cannot accept a completion. Popup windows keep editor focus.

There are 92 curated signatures and descriptions covering the common
financial, lookup, logical, aggregate, dynamic-array, date, text and math
functions listed below. Other available functions show `NAME(…)`, their
catalog category, and an explicit notice that argument details are absent.
Optional arguments use square brackets; ellipses indicate repeating arguments.
The INDEX hint explicitly describes the array form, and the AGGREGATE hint
labels its reference/array alternatives. These are editing hints, not a full
function-argument validation engine.

ABS, AGGREGATE, AND, AVERAGE, AVERAGEIF, AVERAGEIFS, CHOOSE, COLUMNS, CONCAT,
CONCATENATE, COUNT, COUNTA, COUNTIF, COUNTIFS, DATE, DATEVALUE, DAY, DAYS,
EDATE, EOMONTH, EXACT, FILTER, FIND, FV, HLOOKUP, IF, IFERROR, IFNA, IFS,
INDEX, INT, IPMT, IRR, LAMBDA, LEFT, LEN, LET, LOWER, MATCH, MAX, MID, MIN,
MIRR, MOD, MONTH, NETWORKDAYS, NOT, NOW, NPER, NPV, OR, PMT, POWER, PPMT,
PRODUCT, PV, RATE, REPLACE, RIGHT, ROUND, ROUNDDOWN, ROUNDUP, ROWS, SEARCH,
SEQUENCE, SORT, SORTBY, SQRT, SUBSTITUTE, SUBTOTAL, SUM, SUMIF, SUMIFS,
SUMPRODUCT, TEXT, TEXTJOIN, TIME, TODAY, TRANSPOSE, TRIM, UNIQUE, UPPER,
VALUE, VLOOKUP, WEEKDAY, WORKDAY, XIRR, XLOOKUP, XMATCH, XNPV, YEAR, YEARFRAC.

The `AVG` convenience is separate from strict Excel prefix matching. Only
when no actual prefix matches exist does the exact token `AVG` offer available
canonical AVERAGE-family names. The list visibly labels the canonical
suggestion and explains that AVG is not an Excel function. Text remains
unchanged until acceptance. No AVG function or formula evaluator alias was
added.

`setWorkbookCompletions(names, functions)` replaces the workbook context.
Defined names insert their identifier without `(`; supplied workbook functions
insert a function call. Their list labels and tooltips distinguish defined
names from workbook functions, and custom function help explicitly says that
parameter details are unavailable. Function classification takes precedence
when the same identifier appears in both supplied lists. Matching and
deduplication ignore case while insertion preserves the first supplied spelling.
Every frozen built-in name remains reserved, including unavailable built-ins;
workbook entries cannot duplicate or impersonate them. The ordering of the
catalog and workbook-context setters does not change that rule.

Supplied identifiers use defined-name character rules: a letter, underscore or
backslash first; letters, numbers, periods and underscores afterward; at most
255 characters. R/C, actual A1 cell addresses, R1C1-shaped references and malformed
identifiers are rejected. Bounds distinguish valid names beyond the last row
or column from real cell references. Dotted names such as `Sales.Q1` remain
whole identifiers. This API supplies text context only and has no workbook
mutation or VBA execution access.

## TDD evidence

The first RED was compilation against the deliberately new missing public
editor header. Subsequent executed behavior REDs were:

1. Tab did not insert the selected function/opening parenthesis.
2. Down did not navigate suggestions.
3. Nested formula completion was absent.
4. Innermost argument help was absent.
5. Escape did not dismiss suggestions.
6. Assistance remained visible when focus left the editor.
7. LAMBDA lacked a concrete local signature/description.
8. Double-click did not accept a suggestion, using a real first
   press/release followed by a double-click event.
9. Completion incorrectly appeared inside `A$1`.
10. AVG had no explicit canonical suggestion.

Each was followed by the smallest relevant implementation and a passing
focused run before the next behavior was added. Tests use real Qt Test
keystrokes and mouse events, the frozen catalog file, and the actual Rust-backed
engine availability catalog. Twelve scenarios include a 63-function signature
table, a real `QTableView`/`QStyledItemDelegate` editor, no commit on completion
Tab, normal commit on a later Tab, focus/read-only protection, F4 pass-through,
escaped-text/reference boundaries, and available-only AVG suggestions.

The isolated development target lives in `/tmp/tigersheets-formula-editor`;
it builds only this module, its test and the existing C++ calculation adapter,
linking the already-built Rust archive. Root CMake was not changed by this task.
Its focused commands are:

```sh
cmake --build /tmp/tigersheets-formula-editor/build -j2
QT_QPA_PLATFORM=offscreen \
  /tmp/tigersheets-formula-editor/build/formula_editor_tests
```

For repository integration, the test needs Qt6 Test, the calculation adapter,
and `TIGERSHEETS_SOURCE_DIR` defined as the source-root path. Compilation uses
`-Wall -Wextra -Wpedantic`. Offscreen Qt reports its platform limitations for
raising tooltip windows; those messages do not indicate test failure. Desktop
placement and application-level reference picking remain integration checks.

Final observed GREEN: `formula editor tests passed`, exit 0, after the complete
warning-enabled rebuild. The final run also checks that accepting completion
emits inherited `textEdited` for formula-bar/cell-editor live synchronization.
No compiler warnings were produced. A final small refactor avoids asking Qt
to show tooltip windows that are already visible; the complete focused suite
remained green.

## Workbook-context TDD extension

Four additional RED/GREEN cycles added the workbook-context behavior:

1. Compilation first failed because `setWorkbookCompletions` did not exist;
   nested defined-name insertion then passed with no parenthesis or commit.
2. A real-keyboard test failed because workbook functions were absent; UDF
   insertion, explicit kind, deduplication and nested argument context then passed.
3. A registered `GETPIVOTDATA` incorrectly appeared as a workbook function;
   reserving every frozen built-in name fixed this without adding engine support.
4. Valid identifiers beyond actual worksheet bounds were rejected as cells;
   bounded reference recognition fixed them while retaining absolute-reference
   suppression, valid dotted/underscore identifiers and the length boundary.

The resulting suite has 17 scenarios and the original 63 common-signature cases.
It additionally exercises names and UDFs in a real delegate and verifies that
completion changes editor text while the underlying cell model retains its
original value until an explicit commit. The editor does not parse modules,
enable macros, define names, or mutate the supplied workbook context.
Final extension GREEN: the warning-enabled isolated build completed cleanly,
and all 17 scenarios passed with exit 0 and `formula editor tests passed`.

## Primary behavior references

Microsoft documents nested Formula AutoComplete, navigation, Tab/double-click
acceptance, Escape and Alt+Down in
[Use Formula AutoComplete](https://support.microsoft.com/en-us/excel/use-formula-autocomplete).
It describes argument ScreenTips after a function's opening parenthesis in
[Formula tips and tricks](https://support.microsoft.com/en-us/excel/formula-tips-and-tricks).
Signature cross-checks include
[cash-flow functions](https://support.microsoft.com/en-us/excel/go-with-the-cash-flow-calculate-npv-and-irr-in-excel),
[XLOOKUP](https://support.microsoft.com/en-us/excel/functions/xlookup-function),
[LAMBDA](https://support.microsoft.com/en-us/excel/functions/lambda-function),
[SORT](https://support.microsoft.com/en-us/excel/functions/sort-function),
[LET](https://support.microsoft.com/en-us/excel/functions/let-function),
[text functions](https://support.microsoft.com/en-us/excel/text-functions-reference),
and [date/time functions](https://support.microsoft.com/en-us/excel/date-and-time-functions-reference).
Name syntax follows
[Names in formulas](https://support.microsoft.com/en-us/excel/names-in-formulas),
with worksheet bounds from
[Excel specifications and limits](https://support.microsoft.com/en-us/excel/excel-specifications-and-limits).

This module completes the focused function-entry interaction, not all Excel
formula editing. Table-column and locale-specific function-name completion
are not implemented here; catalog availability and caller-supplied workbook
function names remain distinct from calculation conformance and macro permission.


## Compact, screen-bound native suggestions

Two further focused cycles cover native geometry. A 1,230 px formula bar first failed because its completion list used the full field width. It now uses native text metrics and style padding, bounded to 280–520 px and the available screen. Long captions elide without changing the inserted identifier. A bottom/right screen fixture then failed containment of the popup and help. Placement now accounts for native frame margins, keeps both windows inside the available screen without overlap, and also positions standalone argument help. Focus, Tab and Escape remain covered.

Actual raw logs are retained in `validation/ux-rework/tigersheets-formula-{wide,screen}-{red,green}.log`. The final module runs 19 scenarios plus its independent signature expectations. The three inspected native widget grabs are in `output/formula-editor-geometry/`; they are separate actual Qt widgets, not a screen composite.
