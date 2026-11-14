# TigerSheets roadmap

## Delivered development milestone: reliable financial reporting (0.2.0~dev.1)

Repair the confirmed correctness defects, deliver native PivotTables and linked line/column/bar PivotCharts, expose typed Codex proposal operations, and measure the exact A1:J100001 reporting workload. Preserve the complete Excel compatibility release gate. Development installers do not establish complete Excel compatibility.

## Delivered development milestone: forecasts and what-if models (0.3.0~dev.1)

Implemented forecast assumptions, scenario comparison reports, saved one- and two-variable sensitivity tables, circular financing, Goal Seek and Solver workflows with isolated states, retained-link/UDF contexts, convergence qualification, reviewed application and persistence/undo tests. See [the workflow guide](FORECAST_WORKFLOWS.md) and [validation record](FORECAST_RELEASE_VALIDATION.md). The reporting qualification gaps and complete Excel release gate remain in force.

The `0.3.0~dev.2` maintenance iteration adds [worksheet context menus](CONTEXT_MENUS.md) and native merge/dimension editing, with the same complete-release gate.

## Complete-release qualification

Continue the frozen function/shortcut compatibility catalogs, unsupported worksheet objects, native VBA project authoring/debugging/UserForms, additional chart families, accessibility and Ubuntu desktop/installation qualification. Actual Microsoft Excel reopen and VBA execution remain unverified; development verification remains Ubuntu-only.
