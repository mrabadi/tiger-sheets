# Forecasts and what-if models

Open **Data → Forecast**, **Assumptions**, **Scenarios**, **Compare scenarios**, **Data table**, **Goal Seek** or **Solver**. The **Forecasts and what-if models** pane contains the saved setup picker, configuration and explicit preview controls. **New setup** creates an independent setup; selecting a saved setup reloads its fields. Give each setup a meaningful name of up to 64 characters.

Work remains a draft until accepted. **Save setup** prepares an assumption set, scenario, Goal Seek or Solver configuration for review without running the numerical model. For forecast, comparison and sensitivity reports, **Save and preview** computes the report; accept the result to save its definition and generated cells together. **Preview result** runs the selected draft. Ctrl+Enter previews from the pane.

During a run, the pane shows progress and **Stop**. Stop discards that candidate. A successful candidate offers **Accept** and **Discard**; accepting applies one undoable workbook change. Discard retains the previous workbook. If inputs or the prepared calculation context change while work is running, regenerate the candidate. Save the workbook as XLSX after accepting to retain saved setups across reopening.

## Forecast assumptions

Choose the number of periods and first period. Add named drivers with a base and growth value. Each base/growth can be a number, percentage or worksheet reference, such as `1000`, `10%` or `'Operating model'!$B$2`. Select a driver's Base or Growth cell and click **Use selected cell** to bind the worksheet selection. Choose an optional saved assumption set/scenario, number formats and a clear destination worksheet/top-left cell.

The accepted forecast includes visible **Base** and **Growth** columns followed by ordinary compounding formulas. For example, revenue of 1,000 growing 10% produces 1,100, 1,210 and 1,331. An assumption set changes the calculation's trial inputs; authoring or previewing a forecast leaves the live source inputs unchanged. Forecasts support up to 32 drivers and 120 periods.

## Assumptions and scenarios

Use **Add selected input** or enter input addresses. Choose **Number**, **Text**, **Boolean** or **Blank** explicitly. Text retains leading zeros and a leading `=`; it does not become a formula. Numeric percentages are accepted. Each saved set supports up to 1,000 distinct constant or blank input cells. Formula cells and generated report cells are protected.

Save a setup to retain it without applying it. Preview a scenario to review the input changes, then accept to apply them as one change. Existing input number formats remain in place. Use **Delete saved setup** to review removal; remove dependent reports before deleting an assumption set they use.

## Scenario comparison

Choose saved assumption sets/scenarios, reorder the selected cases and optionally include the current workbook baseline. Add model result cells with unique captions and optional number formats. Choose a clear report destination.

Each case starts from the same prepared model, so changes from an earlier scenario do not accumulate into the next one. The report supports up to 32 cases and 32 numeric outputs. The accepted table contains case names and numeric results; the source inputs remain unchanged.

## One- and two-variable sensitivity

Open **Data → Data table** and choose one or two changing input cells. Enter explicit numeric trial values separated by commas, semicolons or new lines; percentages are accepted. Add an optional format for each axis and choose the numeric result cell, caption, result format and report destination.

Each axis supports up to 100 values, with at most 10,000 combinations. In a two-variable report, the first input runs across columns and the second down rows. Each trial uses isolated inputs. Accepting saves the definition and numeric result grid without applying the trial inputs to the source model.

## Goal Seek and Solver

Goal Seek needs a formula output, numeric changing input and target. Optional start and lower/upper bounds restrict the search. Set absolute/relative tolerances and evaluation/iteration budgets as appropriate. Saving the setup does not solve it. Only a converged result offers changing-cell edits for acceptance.

Solver needs an objective cell, a minimize/maximize/target goal, changing cells and any constraints. Add individual selected inputs or a rectangular range, then set bounds, starts and Continuous/Integer/Binary types. Nonlinear and evolutionary methods require explicit finite bounds. Starts must satisfy the input type and bounds. Linear/integer uses HiGHS; continuous nonlinear uses COBYLA; evolutionary/mixed integer uses IHS. Keep the seed with the setup for repeatable searches. There are at most 200 changing cells and 1,000 constraints.

Review the result status and diagnostics before acceptance. Infeasible, cancelled, exhausted-budget, stale or unqualified results do not offer changing-cell edits. An accepted solve changes only its qualified inputs and saved setup; Undo restores the previous workbook.

## Circular financing and source freshness

The **Circular calculation** group lets you review iteration enablement, maximum iterations and maximum change. A maximum change of zero requests exact convergence. Iterative results must converge before numerical analysis can be accepted.

**Circular financing example** prepares two new worksheets with operating cash, capital expenditure, debt and interest formulas, iteration settings and saved financing scenarios/Goal Seek setup. Accept to add the complete example as one change. It chooses distinct worksheet/setup names and retains existing work. With operating cash 100, capital expenditure 150 and interest 10%, debt converges to about 55.5556 and interest to 5.5556.

Comparison and sensitivity result cells are saved numeric snapshots. Select the original model result cell when building another report or choosing a Solver objective or constraint; direct use of those snapshot cells is refused even when their report is fresh. Ordinary forecast formulas and explicit forecast Base/Growth assumptions remain readable. Formula indirection through saved report snapshots is not qualified, and reports are not refreshed recursively during trials.

Input changes mark saved reports **Stale**. The saved picker and selected report status show this state. Preview again to refresh; editing a draft or saving the workbook does not silently replace an existing report. Generated cells are protected; adjust the setup and accept a replacement instead of typing over results.

The **Use saved or unavailable linked-workbook values for this run** checkbox defaults off. Enable it only when saved linked values are an appropriate calculation basis. It grants that run's source choice and is not stored in the setup. The result retains source provenance. Refresh or relocate links when current external values are required.

## File interchange

TigerSheets saved analysis setups, ownership and result provenance use supplemental workbook metadata. Ordinary forecast/source formulas, comparison/sensitivity worksheet values, number formats and iterative calculation settings are written into standard XLSX parts. Other applications can read those worksheet results; they do not receive an Excel-native Scenario Manager, Data Table, Goal Seek or Solver setup from this metadata.

The retained acceptance workbooks in [`validation/forecast-fixtures`](validation/forecast-fixtures) passed two TigerSheets native save/reopen cycles and independent ZIP/XML/openpyxl inspection. Authored formulas currently have no saved Excel formula-value cache; a consuming spreadsheet application must recalculate them. Actual Microsoft Excel open/refresh/save has not been qualified. The existing restriction on exporting edited/authored native VBA projects remains unchanged. This milestone does not change the complete Excel parity release gate.
