# Codex in TigerSheets

Open the right-hand Codex panel and connect with your existing Codex sign-in. The Model, Reasoning and Speed controls follow OwlDocs: capabilities come from the connected runtime, Light is preferred when supported, and Standard is the ordinary speed. Fast is an explicit choice and may use additional credits. Changing a selector does not send workbook contents. Enter sends a message; Shift+Enter adds a line.

For example, ask:

> Generate realistic illustrative data for a fictional company, build a financial model with explicit growth assumptions and formulas, and create an editable forecast revenue graph.

Codex can propose cell values, formulas, styles, worksheets and native charts using the built-in workbook tools. Related tool calls build up one isolated proposal. The worksheet shows the pending result while the original workbook remains unchanged. Use **Accept** to apply the complete proposal as one undoable edit, or **Discard** to remove it. Save the accepted workbook through TigerSheets.

The bridge supplies versioned argument schemas, range-writing and chart examples, bounded workbook reads and searches, and explicit proposal status. Chart additions preserve existing charts. Tool failures return an error without applying a partial edit; stale revisions require a fresh read. Codex cannot accept or save its own proposal or enable workbook macros.

Validation uses deterministic fake app-server processes and the actual Qt MainWindow. It exercises the composed financial model and chart, native editing, Undo and XLSX reopen. It does not establish a live model's response quality or full Excel compatibility. See `TDD_CODEX_CONTROLS.md`, `TDD_CODEX_SETTINGS_PROTOCOL.md`, `TDD_WORKBOOK_TOOLS.md`, `TDD_CODEX_FINANCIAL_WORKFLOW.md` and `validation/codex-workflow` for executed evidence.
