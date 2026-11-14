# Native modeling actions and dialogs

`modelingCommand` connects the application's Goal Seek, Solver, Scenario Manager, Pivot Summary and Two-Variable Sensitivity commands to actual workbook behavior. It uses `WorkbookSession`, `WorksheetModel` and `QTableView` directly; tests do not construct `MainWindow`.

## Interface and behavior

```cpp
bool modelingCommand(QWidget* parent, WorkbookSession&, WorksheetModel&,
                     QTableView&, const QString& command,
                     const Json& args = Json::object());
```

Empty/null arguments open native Qt dialogs with named cell-reference fields, numeric controls, field choices and editable constraint/variable tables. Supplied object arguments execute the same actions without dialogs. Unknown commands and user cancellation return false. Invalid requests throw descriptive exceptions for the host to display. Non-object automation arguments are rejected before any dialog opens.

Every trial calculation uses an isolated snapshot. A successful command sends one operation batch through `WorksheetModel::apply`, so the host receives one `onChanged`, normal revision checks remain active, and one undo restores the entire change. The source revision is checked again before committing. Automation can decline calculated results with `accept:false`.

- `analysis.goalSeek`: `{output:"B1", changing:"A1", target:10}` with optional tolerance/iterations. The changing cell must be numeric. A converged result is shown before acceptance.
- `analysis.solver`: the full `solveWorkbook` specification, plus optional `accept`. The dialog provides objective/goal/method, a changing-cell range, per-variable bounds/integer/binary types, numeric constraints, tolerance and evaluation budget. A feasible result is shown with its changing-cell values before applying one edit.
- `scenario.manage`: `{action:"save"|"apply"|"delete", name:"Base", range:"A1:A3"}`. Save captures constant inputs in workbook metadata; formulas are refused. Apply restores the captured cells as one edit. Existing names require explicit replacement (`replace:true` for automation).
- `pivot.create`: `{range:"A1:B4", rowField:"Region", valueField:"Revenue", aggregate:"sum", output:"D1"}`. The first row supplies unique nonempty field names. Actual calculated records produce grouped values and a grand total. Choices are Sum, Count, Average, Minimum and Maximum. Cells and the TigerSheets definition are saved in one edit. Destinations overlapping source records are refused.
- `analysis.table`: `{rowValues:[10,20], colValues:[2,3], rowInput:"A1", columnInput:"B1", output:"C1", destination:"E10"}`. Row values appear across the top; column values appear down the left. Every interior cell comes from recalculating that input pair. The corner references the original output formula. Input/output overlap is refused; the original model inputs and formula stay intact.

Cell references support active-sheet addresses and quoted worksheet names. Ranges are bounded before expansion. Sensitivity tables allow up to 10000 combinations. Pivot input ranges allow up to 100000 cells. Scenarios allow up to 1000 captured inputs; Solver retains its own variable/constraint limits.

Goal Seek, sensitivity and pivot source reads compose enabled VBA worksheet functions through the actual read-only `evaluateVbaFunction` callback. Disabled VBA is never executed. The synchronous Solver function currently evaluates built-in formulas without a VBA callback.

Pivot outputs are static grouped summaries in ordinary cells, with TigerSheets metadata; native Excel pivot caches are not authored. Sensitivity outputs are calculated snapshots, not native Excel data-table formulas. Both dialogs state the corresponding refresh/interoperability behavior. Analysis remains synchronous; interactive cancellation during a running calculation is not exposed by this helper.

## Observed RED → GREEN evidence

The focused modeling command was a standalone CMake build of the real helper, grid/session, calculation/VBA, analysis and native Solver libraries, followed by:

```sh
cmake --build /tmp/tiger-modeling-build -j2
ctest --test-dir /tmp/tiger-modeling-build --output-on-failure
```

1. **Goal Seek RED:** the deliberately new public `tigersheets/modeling_ui.hpp` interface was missing. **GREEN:** actual `B1=A1*2` calculation seeks target 10, commits A1≈5 once, supports one undo, and preserves the snapshot when declined.
2. **Solver RED:** “Solver action must apply its real feasible solution.” **GREEN:** the helper invokes actual HiGHS through `solveWorkbook`, applies A1=4 for maximum B1=8 once, and leaves a declined solution isolated.
3. **Scenario RED:** “Scenario manager saves selected constant inputs.” **GREEN:** save/apply/delete persists definitions, restores constants without replacing formulas, supports one undo and preserves declined results.
4. **Pivot aggregate RED:** existing `pivotRows` rejected Average with “Unknown pivot aggregate.” **GREEN:** real Sum/Count/Average/Min/Max behavior, text handling and weighted grand totals pass `tests/analysis_tests.cpp`.
5. **Pivot builder RED:** “Pivot builder must create a summary from selected header/data rows.” **GREEN:** East/West revenue groups produce 20 and 7 with total 27; Average produces 10 and weighted total 9. Summary cells and definition are one undoable transaction and source records remain intact.
6. **Sensitivity RED:** “Sensitivity action must calculate and insert every input combination.” **GREEN:** row inputs 10/20 and column inputs 2/3 produce 20/40 and 30/60 in the correct Excel orientation, with one transaction and unchanged source inputs/formula.
7. **Malformed argument RED:** a non-object automation payload unexpectedly opened a dialog. **GREEN:** the public helper rejects it immediately.
8. **Aggregate precision RED:** averaging two finite 1e308 inputs failed through intermediate overflow. **GREEN:** wider accumulation preserves their finite average; unrepresentable sums fail explicitly rather than silently becoming empty JSON values.

## Final verification

`tests/modeling_ui_tests.cpp` also opens and cancels all five real native dialogs, verifies their typed controls and zero mutation, exercises enabled VBA functions in Goal Seek and sensitivity, refuses disabled VBA, and checks atomic failure for formula-changing inputs, duplicate source headers and overlapping destinations. Unknown commands return false without UI.

Final standalone CTest result: **1/1 passed** (all modeling scenarios, approximately 0.3 seconds offscreen). The independent aggregate test executable also passes:

```sh
g++ -std=c++20 -Iinclude tests/analysis_tests.cpp src/analysis.cpp -o /tmp/tiger-analysis-tests
/tmp/tiger-analysis-tests
```

The helper compiles with `-Wall -Wextra -Wpedantic` without warnings. Existing warning messages in the separately owned grid implementation were observed during broad compilation and were not changed here. Qt's offscreen plugin can emit `propagateSizeHints` notices when dialogs open; these do not indicate test failures.
