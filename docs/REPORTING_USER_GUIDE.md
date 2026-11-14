# PivotTable reporting

Select one rectangular source range including its header row. Use distinct, nonempty field names. If header cells contain formulas, choose **Formulas → Calculate now** after editing them. Then choose **Data → PivotTable**. In **PivotTable fields**, check the source sheet/range and click **Read source fields** after changing it or recalculating edited header formulas.

Choose a source field and add it to **Rows**, **Columns**, **Filter** or **Value**. Reorder fields with the arrows or drag within an axis. Values offer **Sum**, **Count nonempty**, **Average**, **Minimum** and **Maximum**; set the caption and number format for each measure. Select a date axis field to group it by year, quarter or month. Choose the destination worksheet/top-left cell, or name a new worksheet, then click **Apply report**. Ctrl+Enter applies from the fields pane.

The report refresh runs with progress and a **Stop report** button. Successful replacement is one undoable change. The previous report remains available if you stop a refresh or calculation fails. If source inputs or the calculation change during a refresh, its candidate is discarded; apply the report again. Generated report values are protected; change the fields/source and apply the report instead of typing over its results.

## Refresh and source status

Source edits mark reports **Needs refresh**. Click **Apply report** in the fields pane to explicitly rebuild from the current source. The status label shows freshness and linked-source status. Saving does not silently refresh a report. A refresh that produces no changed report does not add an Undo entry.

Report filters can include all members or explicit checked members. Search locates cached members; searching does not itself change the filter. Apply the report to commit field, filter or destination changes. Keep the report area clear of other data and reports; conflicting output/source areas are refused.

If a linked workbook is unavailable or only has saved values, TigerSheets asks whether to use those saved values. Choose **Yes** only when that basis is appropriate for the report. The report retains its cached/unavailable source status so the resulting numbers can be reviewed. Choosing Cancel leaves the report unchanged.

## PivotCharts and formulas

In the fields pane, choose **column**, **line** or **bar**, then click **Insert PivotChart**. Clicking a linked chart reopens its report fields. Double-click or F2 edits its title/type; drag to move and use a selected corner to resize. Change its source through the report fields. Refresh updates its data while retaining its title/type/position. Undo restores the table and linked charts together. **Delete report** also removes its linked charts after confirmation.

While entering a formula beginning with `=`, point at a report value to insert `GETPIVOTDATA`. Shift-selection of multiple cells inserts an ordinary A1 range. For a report anchored at `Report!A1` with a measure caption `Revenue`, a lookup can be written as:

```text
=GETPIVOTDATA("Revenue",Report!$A$1,"Region","East")
```

Use the actual measure caption and source field/member names. Formula lookups follow the report's current cached result; refresh the report after changing its source.

**Formulas → Calculate now** updates ordinary worksheet formulas. **View → Show formulas** displays their source while numeric selection totals and **Paste Values** keep using typed calculated values. Literal numbers retain their number formats. Switching worksheets keeps current results available; stale calculation work runs in the background. Reports still refresh explicitly through **Apply report**.

## Convert an older summary

Choose **Data → Convert summary…** and select the older static summary. TigerSheets prepares a native report on a new worksheet and shows an **Accept / Discard** preview. Accept creates the editable native report; Discard leaves the workbook unchanged. The original summary cells are retained, and one Undo reverses the accepted conversion. Older summaries are not silently converted during open or save.

## Current bounds

| Item | Supported bound |
| --- | --- |
| Source | 100,000 data records × ten fields, plus the header row |
| Source text | 32,767 characters per cell and 128 MiB decoded source text |
| Measures | Four per report |
| Generated output | 250,000 cells within worksheet bounds |
| PivotChart drawing | 1,000 categories and 16 series |
| Printing | 10,000 rows, 200 columns and 250,000 selected cells |

A report exceeding the output bounds is refused with a diagnostic; filter members or remove fields. A table exceeding the chart drawing bounds is still saved completely, with its linked chart retained and drawing suspended until the table becomes smaller. Nothing is silently truncated. Imported reports/charts using unsupported native features remain read-only or protected from unsafe edits.

These native XLSX structures have independent package/parser coverage. Actual Microsoft Excel refresh/reopen, physical printers, complete Excel parity and native Wayland keyboard qualification remain unverified; see [release validation](RELEASE_VALIDATION.md).
