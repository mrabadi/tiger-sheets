# Codex financial authoring: desktop acceptance test

Date: 2026-11-11. The test is
`tests/codex_financial_workflow_tests.cpp`, scenario `financial`.

The fixture uses a deterministic local fake app-server process. The actual
MainWindow composer, Send button, Codex protocol client, workbook tool bridge,
Accept control, calculation engine, native chart editor, and XLSX writer/reader
participate. No account, external service, or network access is used.

The composer sends the exact reported prompt asking for realistic financial data,
a financial model and a forecast revenue graph for a fictitious company. The
deterministic worker chooses fictional Cedar Ridge Analytics. The worker discovers operation schemas and a financial
model example, reads the current revision and sheet identity, proposes the model,
then proposes its chart in a second tool call. It reads and searches pending data
before returning its streamed response. Both proposals must stay isolated until
the user accepts them together.

Independent expectations are historical revenue of $12 million in 2024 and
$15 million in 2025, followed by explicit 18% annual growth: $17.7 million,
$20.886 million, and $24.64548 million in 2026–2028. EBITDA uses a separate 22%
margin assumption. Number and header formatting must survive acceptance and XLSX
reopening. The graph must use calculated forecast values, allow selection and
editing through the normal chart editor, and preserve the edited title when saved.
One Undo must remove the complete accepted model and graph; Redo restores them.

## Execution evidence

The root task owns production integration and test-target registration. The source
passed an explicit Qt 6 Widgets/Test C++20 syntax check. This was setup validation,
not RED/GREEN behavior evidence. An initial fake-process failure-file publishing
race caused an empty diagnostic; atomic file replacement repaired this fixture
before establishing the meaningful RED below.

RED command, executed by the root task before production bridge integration:

```sh
QT_QPA_PLATFORM=offscreen build/codex_financial_workflow_tests financial
```

Observed exit status: 1. Observed diagnostic:

```text
FAILED: capabilities must publish versioned operation schemas for workbook authoring
```

The fake worker reached the actual desktop capabilities callback and could not
discover a usable workbook authoring contract. That initial RED preceded the subsequent GREEN execution recorded below.

GREEN: after binding the owned WorkbookTools bridge to the desktop, the same
command printed `Codex financial workflow passed` (exit 0). The real native
chart editor and XLSX reopen checks participated.

A subsequent focused check required the pending read to include the calculated
2026 revenue as well as its formula. The same command failed with:

```text
FAILED: Codex must be able to read calculated pending forecast values
```

The desktop now supplies its existing recalculation path to the readback
provider, including workbook UDF/link handling and revision-tagged diagnostics.
After rebuilding, the same command again printed `Codex financial workflow passed`
(exit 0). No live model or account was used.

A passing characterization check asserts that the worksheet shows `$17,700,000`
after the chart editor's scheduled recalculation completes, rather than checking
an intermediate raw-formula state.

Historical local visual inspection also found the streamed assistant body attached to the Codex
role label. A focused assertion failed with `streamed assistant text must start
on its own line below the Codex role`. The panel now inserts a fresh text block
and resets the streaming text format. Rebuilding the optimized target and running
the same financial command passed. The saved workbook is retained as
`validation/codex-workflow/Cedar-Ridge-Analytics.xlsx`; the historical local
render is not included in the repository or needed for these assertions.
