# Release validation status

**The complete Excel-compatible release remains blocked.** The current installer is `0.3.0~dev.2`, adding worksheet right-click menus; see [context-menu validation](CONTEXT_MENU_VALIDATION.md). The forecasts and what-if milestone is `0.3.0~dev.1`. See [its validation record](FORECAST_RELEASE_VALIDATION.md) for current numerical, UI, persistence, worker, Codex and package evidence. The reporting evidence below remains the historical `0.2.0~dev.1` baseline.

**The complete Excel-compatible release remains blocked.** The reporting baseline is the labeled `0.2.0~dev.1` reporting development build. Native PivotTables, PivotCharts and the correctness fixes below have executed focused evidence. The final optimized application suite passes **82/82 CTest targets** (zero failures, 22.96 seconds) after the report-pipeline fixes. The final optimized desktop rerun passes the selected report-action and input limits on the recorded host. The development `.deb` passes private extraction/runtime inspection and read-only upgrade simulation; an actual system installation remains unperformed. The earlier `0.1.0~dev.2` installer remains historical evidence, not validation of this revision.

The ordinary CPack configuration still invokes the complete compatibility gate. Only the explicitly named development configuration can package this incomplete implementation. No required function, shortcut or feature was excluded to qualify the reporting iteration.

## Reporting baseline (0.2.0~dev.1)

| Acceptance slice | Executed evidence | Qualification boundary |
| --- | --- | --- |
| Native reporting | Genuine XLSX PivotTable definitions, typed caches, source bindings, row/column/filter fields, five aggregates, multiple measures, date grouping, refresh/update/delete and atomic ownership tests | Supported subset; unfamiliar native objects remain read-only. Actual Microsoft Excel refresh/reopen is unverified. |
| Source capacity | Exact **100,000 records × ten fields**, one million data cells plus headers; two retained native pivots; calculated inputs, blanks, typed values and formats; all five aggregates checked | Headless pipeline on the recorded host, not the four-core/16-GiB reference configuration. |
| Native PivotCharts | Genuine line/column/bar PivotCharts linked to native tables; three charts, **960 numeric cached points checked per file**, two save/reopen cycles | More than 1,000 categories or 16 series suspends chart drawing/cache while preserving the full report and chart identity. Unsupported chart features are preserved read-only. |
| Report interaction | Field pane, isolated report worker, immutable job context, cached-link consent/provenance, GETPIVOTDATA lookup/pointing, prepared previews and one-transaction acceptance | One authenticated fictional-data Codex report workflow passed reviewed acceptance, Undo/Redo and native save/reopen; broader model/tool and Excel interaction parity remain unqualified. |
| Everyday correctness | Typed Paste Values, stale-cache refusal/clearing, atomic cut/dependent references, stable calculated-key sorting, whole-axis rebasing, nonconvergence refusal, shared financial formatting and scatter numeric X | Partial-overlap/chart/pivot-dependent moves explicitly refuse within this iteration's bounded contract. Broad structural reference parity is incomplete. |
| Printing | Actual PDF chart graphics, used/explicit areas, fit-width, repeated headers, orientation, native Print_Area/Print_Titles roundtrip with rename/reorder/delete | Up to 10,000 rows, 200 columns and 250,000 selected cells; larger jobs refuse. Physical printers and complete Excel print-layout parity are unqualified. |
| Persistence and recovery | Styled-save indexing, opaque chart preservation, unique native parts/IDs, SaveAs pivot binding, background recovery/deltas, crash recovery and worker failure tests | Full power-loss/interrupted-save campaign remains incomplete. Genuine newly authored/edited MS-OVBA projects are not qualified. |
| Optimized application suite | **82/82 CTest targets**, zero failures, 22.96 seconds; application/worker identities and CTest XML recorded | Final source development coverage; complete compatibility acceptance remains a separate gate. |
| Focused sanitizers | Eleven worksheet/session/reference/scheduler/chart/print/native executables passed ASan+UBSan; latest pure cache, instrumented native child/protocol and actual warm-lifetime paths also pass. Recovery/prepared history have separate evidence | Qt leak detection disabled; focused runs do not cover every application path. Earlier transient mixed-layout failures are retained and distinguished from passing reruns. |
| Native Ubuntu interaction | Focused **X11/xcb** keyboard completion, formula pointing, Escape and Qt accessibility role/name/value/focus passed | Wayland loads/exposes but this compositor does not grant programmatic keyboard focus. **Wayland is not qualified.** No screen-reader/AT-SPI or complete desktop keyboard acceptance is claimed. |
| Development package | Actual `.deb` private-stage manifest/ELF/RUNPATH/Qt/desktop/MIME/icons, app version, both worker protocols and read-only APT upgrade simulation pass; validator has five passing tests | This host uses its installed runtime. No actual system installation/upgrade or clean-machine qualification is claimed. |

The [reporting guide](REPORTING_USER_GUIDE.md) describes the UI. The trace index is [REPORTING_REGRESSIONS.md](REPORTING_REGRESSIONS.md). Raw test-first logs are in [reporting-tdd](validation/reporting-tdd); [the original exact-capacity record](validation/reporting-capacity/reporting-capacity.json) and [the compact-output rerun](validation/reporting-capacity-computed/reporting-capacity.json) contain timings and executable SHA-256 identities; host-specific details are redacted from the public records. The [final 82-target CTest log](validation/reporting-tdd/persistence-performance-final-ctest.log), [CTest XML](validation/reporting-tdd/persistence-performance-final-ctest.xml), [all-target build log](validation/reporting-tdd/persistence-performance-final-build.log) and [binary identities](validation/reporting-tdd/persistence-performance-final-identities.json) retain the executed application evidence. The [candidate package audit](validation/reporting-tdd/development-package-candidate-validation.json) retains its verified checksum and 20 executed commands; final docs-inclusive bytes are audited again in `validation/reporting-tdd/development-package-validation.json`, with distribution checksums produced externally in `dist/SHA256SUMS`.

## Capacity and performance evidence

The original complete reporting run used `A1:J100001`, including 100,000 data records, ten nonempty header fields, 10,000 source formulas and 14,286 blank source inputs. Both pivots retained all 100,000 typed cache records. The independent ZIP/OpenXML/openpyxl inspector checked both saved files, native relationships, qualified chart source names, rendered totals and all 960 chart numeric cache points in each file. The small enabled-UDF/retained-link fixture independently checked all report intersections and totals; native VBA source export was not part of that qualification.

| Measurement | Original full pipeline | Compact-output rerun |
| --- | ---: | ---: |
| Source calculation / report generation | 10,544 / 2,114 ms | **4,526 / 1,796 ms** |
| Calculation plus generation | 12,658 ms | **6,322 ms** |
| Initial worker / create including validation | 11,716 / 12,919 ms | 11,172 / 12,726 ms |
| Filter regeneration / apply | 1,826 / 7,287 ms | 1,262 / 6,352 ms |
| Refresh from current cache (minimum aggregate) | 428 ms | 574 ms |
| No-op refresh apply / undo | 136 / 132 ms | 163 / 139 ms |
| First / second native save | 24,029 / 31,830 ms | Not rerun |
| First / second native reopen | 10,216 / 15,008 ms | Not rerun |
| Sampled process-tree peak RSS | 4,686,968 KiB (4.47 GiB) | **2,795,684 KiB (2.67 GiB)** |
| Native file sizes | 7,804,468 / 7,804,877 bytes | Not rerun |

The compact run passes exact-capacity calculation/report correctness. It emits 10,000 calculated values for 10,000 formula cells, retaining exceptional typed literals where raw reconstruction would differ; ordinary literals use validated source fallback. These historical stage timings are useful attribution; they do not establish the whole-action fresh/warm UI budgets. The visible desktop qualification below measures complete report actions through final viewport repaint, separately from native save latency.

The compact run deliberately covers calculation/report/refresh only. Native save/reopen, recovery and independent native inspection remain the original complete-pipeline evidence; they were not silently relabeled as compact-run measurements. Saves/reopens remain substantial operations. The full baseline headless pipeline took 142 seconds; the narrower compact path took 34.5 seconds. Independent inspection is a separate validation step.

Host-specific hardware and system details have been redacted from the public evidence. `referenceMachineMatch` remains false. These measurements do not qualify the specified four-core/16-GiB reference configuration.

The separate original modeling fixture has 100,000 cells, 20,000 formulas and 20 sheets. Its final calculation runs took **609.858 / 557.635 / 561.427 ms** ([record](validation/reporting-desktop/final-recalculation-100k.json)). Its direct model-edit timings exclude rendering and recovery, so they do not establish the input-to-paint target. A separate one-million-cell/200,000-formula/20-sheet calculation stress run takes **6,039.809 ms** ([record](validation/reporting-desktop/final-stress-1m.json)); it does not include the report action or an input-to-paint measurement.

The exact selected **100,000-record × ten-field** GUI input fixture now passes **22.048-ms input-to-painted-feedback p95**, covering ten existing-cell edits and ten insertions into a new adjacent column. The prior 73.942-ms failure is retained. The shown offscreen Qt window forces a viewport repaint; process-tree peak RSS was 1.49 GiB. See [final input log](validation/reporting-desktop/final-input.log) and [measurement/binary identities](validation/reporting-desktop/final-summary.json).

The latest visible report pipeline fixture also uses **one million data cells plus ten headers**, with one formula retained through the source edits. It runs an actual Qt event loop and includes cooperative input capture, worker serialization/process/response, prepared publication, fields-pane updates and final viewport repaint. The warm action changes the source formula, completes a shared calculation before starting its timer, then refreshes the changed report; it is not an unchanged no-op.

| Whole report action | Budget | Final optimized executable | Longest UI dispatch |
| --- | ---: | ---: | ---: |
| Initial creation | 15,000 ms | **5,131 ms** | 24.905 ms |
| Fresh refresh after source formula edit | 10,000 ms | **6,418 ms** | 43.723 ms |
| Changed source with matching completed calculation | 3,000 ms | **2,278 ms** | 32.853 ms |
| Unchanged current report refresh | 3,000 ms | **2,008 ms** | 31.725 ms |

All four actions pass the 100-ms UI-dispatch limit. Totals, one-step report Undo/Redo and unchanged-refresh revision/history are checked. The optimized warm path is qualified here only with **no workbook links or native external references**; linked contexts use the full isolated calculation path. Calculation replacement during a sliced warm capture or later in the job cleanly discards the candidate without workbook/history changes ([lifetime regression](validation/reporting-tdd/worksheet-report-lifetime-expanded-green.log)). Latest ASan+UBSan checks also pass for [pure cache extraction](validation/reporting-tdd/analysis-current-cache-pivot-sanitizer.log), [front/native-child protocol](validation/reporting-tdd/analysis-current-cache-worker-sanitizer.log) and [both actual warm result-replacement boundaries](validation/reporting-tdd/analysis-current-cache-lifetime-sanitizer.log), with leak detection disabled and undefined behavior configured to halt. Latest native prepared-transaction allocation/restoration/rollback sanitizer cases also [pass](validation/reporting-tdd/analysis-current-native-prepared-sanitizer.log); the [manifest](validation/reporting-tdd/analysis-current-cache-sanitizer-identities.json) records exact source/object/binary/log identities and instrumentation boundaries.

[Final four-phase log](validation/reporting-desktop/final-publication.log) and [commands/executable identities](validation/reporting-desktop/final-summary.json) use the optimized all-target Release executables. Final viewport paints take 7–12 ms, and sampled publication process-tree peak RSS is **2.51 GiB**. The window is shown through the offscreen backend, so these timings are not native Wayland qualification or UI-to-durable-save latency. The earlier [private-object run](validation/reporting-desktop/profile-visible-warm-identities.json) is retained with its explicit boundary and is superseded by this final optimized run.

Preparation attribution separately measures **214.49 ms** for the owned-source transaction factory and **89.86 ms** for canonicalization, retaining the original one-million-cell source allocation ([record](validation/reporting-tdd/persistence-native-prepare-profile-green.log)). Earlier 24.472-second/130.526-ms publication and 7.646-second warm failures remain in the desktop trace directory. The final pipeline test replaces a polling harness with a real event loop and records stage/microgap timings; it does not lower the selected source capacity. No result qualifies a 16-GiB machine by extrapolation. The separate [quiet exact-million-cell native-report history check](validation/reporting-tdd/persistence-native-history-quiet-final.log) measured **126.44-ms Undo** and **35.54-ms Redo** while retaining the source allocation. The **100-ms Undo target is unmet**; the report-action UI timings do not qualify every operation below 100 ms. Earlier million-cell stress numbers in `performance-release-1m.json` remain historical.

A bounded [authenticated live report workflow](validation/reporting-live/acceptance.json) also passed using six fictional records: the model created a native PivotTable and column PivotChart, totals were checked, the desktop reviewed/accepted the proposal, one Undo/Redo reversed/restored it, and native XLSX parts were verified after save. Its runtime/model and executable hashes are recorded separately. It does not qualify unrelated model operations.

## Development package audit

The actual candidate `.deb` passes private extraction, package/version/channel/dependencies, desktop/MIME/eight PNG icons, all staged ELF dependencies, relative bundled solver runtime resolution, installed Qt platform plugins, offscreen app version, native report-worker output and private file permissions, disabled-macro worker protocol, and read-only APT upgrade simulation. APT proposes **one upgrade (`0.1.0~dev.2` → `0.2.0~dev.1`), no new packages and no removals**. No installed package or host setting was changed.

The initial audit exposed a verifier string-comparison defect: loader paths containing `/usr/bin/../lib` were correctly inside the private stage but were rejected before path normalization. The original diagnostic is retained; a realistic loader-path RED and five-test GREEN correct the verifier, and the same package then passes ([RED](validation/reporting-tdd/worksheet-package-origin-red.log), [GREEN](validation/reporting-tdd/worksheet-package-origin-green.log), [actual audit](validation/reporting-tdd/development-package-candidate-validation.json)). No application/package fix was needed. The installed app hash changes from the build-tree hash because installation rewrites RUNPATH; the two worker hashes remain equal. Final external checksums identify the docs-inclusive delivered bytes without attempting to embed a package's own checksum inside itself.

## Complete-release contract and remaining limits

The final pinned engine run passed **2,767 unit tests** and **23 documentation tests**, with eight ignored; the owned Rust bridge passed **four tests** after direct typed parsing. See [final engine log](validation/reporting-tdd/analysis-upstream-final-typed-parse.log) and [final bridge log](validation/reporting-tdd/analysis-bridge-final-typed-parse.log).

The frozen contract remains **512 function rows (497 required, 15 excluded)**, **226 Windows shortcut rows (190 required, 36 excluded)** and **21 required feature groups**. Source URLs, capture dates, categories and exclusions remain in the compatibility catalogs. Availability alone is not conformance. Historical upstream results (2,765 unit tests and 23 documentation tests, eight ignored) and the 282-case independent corpus do not establish all Microsoft semantics.

| Area | Material remaining complete-release gaps |
| --- | --- |
| Calculation | Complete function/names/arrays/cycles/volatility/calculation-mode conformance. 1904 calculations explicitly refuse rather than silently produce incorrect dates. |
| Worksheet | Full structural reference edits; tables, validation, filters, conditional formatting, protection, grouping, merges; complete Windows shortcuts/KeyTips/remapping/accessibility. |
| Native interoperability | Broad imported-object editing and opaque dependencies; full native Solver/scenario/data-table interoperability; actual Microsoft Excel reopen/refresh and VBA execution. |
| Charts and analysis | Remaining chart families, combinations and rendering parity; broader statistical/financial analysis and solver interoperability. Forecast/what-if expansion is reserved for follow-on work. |
| VBA | Full language/object model, classes/Variant/ByRef/events/debugger/UserForms; genuine authored/edited native project export and signature/cache invalidation qualification. |
| Workbook links/Codex | Full open-workbook coordination/external names/cross-workbook cycles; broader authenticated model/tool qualification and complete operation coverage. |
| Robustness/platform/delivery | Sustained coverage-guided fuzzing, complete crash/performance campaign, clean-machine install/upgrade, physical printer, Wayland and assistive-technology acceptance, distribution audit. |

The [actual final full-release CPack attempt](validation/reporting-tdd/persistence-performance-final-release-gate.log) exits 1 with **RELEASE BLOCKED** because complete acceptance evidence remains missing. The current reporting work does not weaken that gate. A labeled prerelease installer is useful for evaluation; it does not qualify complete Excel/VBA compatibility.

## Historical evidence

The prior Codex revision passed 43/43 CTest targets and eight focused sanitizer suites; the earlier workspace UX revision passed 37/37. Their binaries, screenshots and package audits remain in [codex-workflow](validation/codex-workflow) and [ux-rework](validation/ux-rework). The original 28-target build, independent financial workbook, upstream engine run, release-gate refusal and temporary-prefix installation remain under [validation](validation). These records are retained with their original limitations and binary identities and are not claimed as final checks of `0.2.0~dev.1`.
