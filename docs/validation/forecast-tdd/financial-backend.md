# Financial model backend evidence

Scope: the version1 financialModels collection; typed assumption sets and scenarios; formula-based growth forecasts; saved comparisons; one/two-variable sensitivity; explicit preview operations; calculation-context, cancellation, convergence and managed-output validation. No source model is applied by generation. Candidate cell storage is copied once per analysis, then declared input slots are reset in place between cases.

The focused commands compile tests and owned source under `/tmp` with the existing release calculation/formatter/reference archives and pinned Rust bridge. Workflow tests additionally link the existing Goal Seek/Solver archives and bundled solver libraries. Native tests launch an independently linked actual report-worker child through ReportWorker; they do not replace calculation with a fake process.

| Cycle | Observed RED | Observed GREEN / independent oracle |
| --- | --- | --- |
| Typed assumptions | assumptions-red.log: new API undefined | assumptions-green.log: constants only; string/formula-looking text preserved; formulas never overwritten |
| Forecast | forecast-red.log: builder absent | forecast-green.log:100 ×1.1³ =133.1; linked Base/Growth and visible compounding formulas |
| Scenario comparison | scenario-comparison-red.log: kind absent | scenario-comparison-green.log: current/upside/margin22/44/55; unrelated inputs reset per case |
| Sensitivity | sensitivity-red.log: kind absent | sensitivity-green.log:110/220 and110/220/120/240;3/5 evaluations including baseline |
| Ownership | ownership-red.log: manual edit permitted | ownership-green.log: generated output protected; styling preserved across refresh; source edits stale; unchanged refresh no operation |
| Publication | publication-red.log: expanded ownership rectangle accepted | publication-green.log: exact shape, complete grid, formula/input identity, finite result dimensions and owner tags checked |
| Native collisions | native-collision-red.log: pivot feedback accepted | native-collision-green.log: pivot source/output and saved-input rectangles protected |
| Workflow | workflow-red.log: run unsupported | workflow-green.log: Save/Run/Apply/Delete form one review batch; normalized Goal Seek changes20 for output40 |
| Iteration / example | settings-red.log: settings action absent | settings-green.log: merged settings, no calculation, unrelated settings retained; financing example is only preview operations |
| Native worker | native-worker-red.log: financial_model rejected | native-worker-green.log: actual child forecast133.1 and bound revision/context/workbook identity |
| Native lifecycle / provenance | native-lifecycle-red.log: numerical provenance absent | native-lifecycle-green.log: comparison110/220, sensitivity240, Goal changing200, feasible Solver, retained links+VBA13.31, cached-value grant/refusal and no-evaluation settings/example |
| Trusted tool context | native-tool-context-red.log: linked save lacks trusted prepared context | native-tool-context-green.log: actual child uses host-provided frozen-link context for save-only normalization |

`*-initial-oracle*` and `*-attempt*` artifacts are diagnostic attempts, not successful RED/GREEN evidence. The first native lifecycle passing attempt used the wrong Solver reply key (`feasible` rather than the actual `feasibility`); the corrected final run preserves the numerical assertion. The first tool-context request omitted the schema-required version; that request was corrected before the behavioral RED and passing implementation check.

Bounds: forecast1–120 periods and1–32 drivers; assumptions/scenarios1–1000 typed constants; comparisons1–32 cases and1–32 labeled numeric outputs; sensitivity1/2 distinct numeric inputs,1–100 samples each and at most10000 combinations; generated reports at most20000 cells. Unsupported definition versions remain read-only. Financial setup/report metadata is application metadata in XLSX; native cell formulas/values remain authoritative and changed native-package fingerprints discard private metadata.

Remaining qualification is recorded separately: full integration, installer execution, sanitizer runs and measured capacity depend on the final shared build. These focused checks do not claim Excel-native forecast/Data Table interoperability or completion of the full Excel/VBA release gate.

Final integration refinements:

- `workflow-controls-red.log` / `workflow-controls-green.log`: engine limits are1–100000 iterations and finite nonnegative maximum change (zero means exact convergence); identity-only saved actions resolve canonical definitions; example accepts an empty definition.
- `native-tool-full-results-red.log` captures actual Solver failure when sparse outputs omit a raw numeric constraint. `native-tool-full-results-green.log` records the actual child returning objective165 with the independent input bound150 after all four financial tool operation names select full results.
- `native-report-provenance-red.log` / `native-report-provenance-green.log` show review summaries receive the same link/permission provenance already persisted in report definitions.
- `metadata-authority-red.log` independently reproduces both shared-string-only and number-format-only native edits retaining stale app definitions, with unchanged worksheet XML. `metadata-authority-green.log` runs the real corrected IO source and confirms native edits survive while private financial definitions are discarded.
- `package-verifier-red.log`, `package-financial-verifier-red.log` / `package-verifier-green.log`: seven Python contracts now require development version0.3.0~dev.1, the report worker's own relative bundled solver RPATH and bundle resolution, plus qualified staged financial Solver smoke reply validation. Actual installer extraction/execution is a separate artifact-specific check.

Focused ASan+UBSan backend test run passed (`backend-sanitizer.log`). Only the new financial model backend and focused test are instrumented; release calculation/pivot/formatter/reference/Rust dependencies are linked normally. The run precedes the separately owned manual-number-format-preservation refinement; source/binary identities and this qualification boundary are in `backend-sanitizer-identities.json`.

The final frozen backend source and expanded tests were subsequently rebuilt and rerun under ASan+UBSan: PASS (`backend-final-sanitizer.log`, exact identities in `backend-final-sanitizer-identities.json`). This final run includes manual-format preservation, explicit driver-format override and stale/fresh cached-report source refusals. It supersedes the earlier pre-refinement sanitizer boundary; linked dependency archives remain uninstrumented and leak detection is disabled.
