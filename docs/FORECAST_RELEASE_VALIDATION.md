# Forecasts and what-if models: development validation

Candidate version: **0.3.0~dev.1**, Ubuntu amd64. This milestone extends the reporting development build. The complete Excel compatibility release gate remains **BLOCKED**; no compatibility catalog entries, performance targets or interoperability obligations were removed.

## Implemented and exercised

- Saved typed assumptions/scenarios; linked, auditable forecast formulas; independent scenario comparison; saved one/two-input sensitivity grids.
- Saved Goal Seek/Solver configurations, qualified numerical results, explicit iteration settings and a non-destructive circular-financing example. Failed, limited, stale, cancelled and nonconverged calculations cannot propose changed inputs.
- A compact native authoring pane, keyboard controls, bounded Codex financial operations/reads and runtime schema parity. Jobs capture workbook identity, revision, calculation generation and explicit cached-link consent. Calculation uses retained links and enabled read-only VBA functions.
- Isolated child computation and background transaction preparation, reviewed Accept/Discard, one undoable acceptance, source freshness, generated-cell protection, typed deletion, recovery-journal freshness and unchanged-refresh history preservation.
- Two native XLSX save/reopen cycles, stable authored worksheet identities and generated output ownership. Supplemental metadata is discarded after native part changes, including independent shared-string and style edits. Manual report formatting survives changed-source refresh; an explicit configured format change still applies.

The [workflow guide](FORECAST_WORKFLOWS.md) explains the controls and limits. [Focused RED/GREEN evidence](validation/forecast-tdd/README.md), [numerical/tool evidence](validation/forecast-tdd/analysis.md), [UI and independent workbook evidence](validation/forecast-tdd/ui-validation.md), and [final pipeline review](validation/forecast-tdd/final-review.md) retain the executed commands and boundaries. The [fixtures](validation/forecast-fixtures) include ordinary formulas and report values plus editable TigerSheets setups. Actual native-worker desktop tests exercise preview/accept, saved refresh, Undo/Redo, independent comparison/sensitivity, the circular example and deletion.

## Final qualification

The final consistent optimized build passes **104/104 CTest targets**, zero failures, **66.33 seconds**. The preceding full run also passed104/104; the final run includes the cached-report chaining guards. `final-ctest.xml`, `final-ctest.log` and `final-no-change-build.log` retain the final result and no-work rebuild. `final-source-manifest.json` hashes757 source, schema, compatibility, packaging, test and pinned-engine files. Focused UI, controller and final financial-builder ASan/UBSan checks pass with documented instrumentation boundaries and Qt leak detection disabled.

Quiet-host final optimized measurements:

| Measurement | Result | Boundary |
| --- | ---: | --- |
| 100,000 cells /20,000 formulas /20 sheets | 459.08 /426.67 /433.41ms | Complete calculated values checked; all below1second |
| A1:J100001 input-to-paint p95 | 44.48ms | Twenty actual existing-cell/insertion edits plus synchronous viewport paint; below50ms |
| A1:J100001 initial native report | 8.379s | Complete captured source, calculation and publication; below15seconds |
| Full refresh | 8.447s | Required source calculation and publication; below10seconds |
| Already-current calculated source refresh | 2.741s | Complete action; below3seconds |
| Unchanged refresh | 2.666s | No history/revision change, totals validated |
| Largest observed refresh UI dispatch | 91.68ms | Below the benchmark's100ms dispatch bound; separate input p95 is above |

Host-specific hardware and system details have been redacted from the public evidence. These measurements do **not** qualify the specified reference configuration. Raw measurements are in `validation/forecast-tdd/{calculation-100k,input-100k-records,reporting-100k-records}.*`. GNU time's maximum RSS was1,565,140KiB for the input run and2,111,756KiB for report publication; this is not a fresh sampled aggregate process-tree memory qualification.

The installer's exact package SHA256 and staged-audit status are published in `dist/SHA256SUMS` and its adjacent `.deb.validation.json`, outside the archive to avoid a self-referential checksum. The audit checks private extraction, manifest/ELF/runtime resolution, both worker protocols including an actual financial Solver request, icons/MIME/desktop entries, Qt plugins and read-only APT upgrade simulation. It does not install or upgrade the host system.

Reporting measurements and outstanding qualification from [the reporting baseline](RELEASE_VALIDATION.md) remain historical evidence. This milestone does not qualify the required four-core/16-GiB reference machine. New results must identify their exact measurement boundary; host-specific details may remain in private records.

## Interchange and remaining limits

Financial setups/provenance use fingerprint-checked supplemental metadata. Standard XLSX worksheet cells, ordinary forecast formulas, formats and native iteration settings remain readable without it. These setups are **not native Excel Scenario Manager, Data Table, Goal Seek or Solver definitions**. Authored formula values require consumer recalculation. Actual Microsoft Excel reopen/save, VBA execution and authored native VBA projects remain unverified or incomplete.

Verification is Ubuntu-only. Clean-machine installation/upgrade, complete Wayland keyboard/assistive-technology qualification, all Excel formulas/shortcuts/objects, and the complete release contract remain outstanding. The development package uses explicit development labeling; ordinary release packaging remains fail-closed.

The current native X11 smoke attempt could not connect to the inherited display from this sandbox (`native-x11.log`); new UI/desktop acceptance uses Qt offscreen. The prior reporting X11 evidence is historical. No new Wayland qualification or authenticated live Codex financial-tool workflow was performed; deterministic tool/native-process workflows are covered.
