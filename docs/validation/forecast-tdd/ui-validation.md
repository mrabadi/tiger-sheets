# Financial pane validation

The pane authors bounded definitions and delegates all computation, prepared snapshots and workbook mutations to the host. Existing synchronous `modelingCommand` compatibility remains available; normal toolbar authoring uses the new pane and asynchronous host.

Each new behavior below was implemented after its focused failing regression. `ui-*-red.log` records the observed assertion/API failure and `ui-*-green.log` records the passing test. Setup/dependency errors are excluded from RED evidence.

| Behavior | Evidence prefix |
|---|---|
| Review controls, running/preview locking, explicit Stop/Accept/Discard | `ui-lifecycle` |
| Forecast source bindings, percentage parsing and isolated run callback | `ui-forecast` |
| Typed scenarios, literal formula-looking text, unique inputs | `ui-scenario` |
| Generated report Save and preview/accept-to-save contract | `ui-report-save` |
| Context refresh preserves the edited draft/destination | `ui-draft-context` |
| Saved setup selection, independent New and reviewed Delete | `ui-saved-setups`, `ui-new-picker` |
| Ordered scenario comparison membership and baseline | `ui-comparison` |
| One/two-variable inputs, explicit finite percentage trials | `ui-sensitivity` |
| Saved Goal Seek settings and invalid-bound refusal | `ui-goal-seek` |
| Saved Solver bounds/start/type/constraints and integer validation | `ui-solver` |
| Default authoring drafts for all seven workflow kinds | `ui-defaults` |
| Explicit unchecked cached-linked-source consent, host reset | `ui-cached-consent` |
| Embedded iteration review/example callbacks, draft preservation | `ui-iteration-embedding` |
| Optional formats/scenario binding retain saved meaning | `ui-optional-metadata` |
| Financial name limit matches persisted 64-codepoint bound | `ui-name-bound` |
| Saved stale status, draft preservation and running/preview priority | `ui-stale-status` |

`ui-keyboard-verify.log` adds a native Qt characterization of Ctrl+Enter preview, Space for Stop/Accept and Tab to Discard. `ui-sanitizer.log` passes the complete focused pane suite with address/undefined sanitizers; pane, iteration widget and test objects were instrumented, system Qt was not. Leak detection was disabled for the system Qt process. `ui-sanitizer-build.log` records the compiler output. This is an offscreen Qt test, not Wayland desktop qualification.

Historical local renders of the standalone pane at 440×900 using native default Qt styling were inspected for scrolling configuration pages and tables, persistent review actions, readable source consent and keyboard access. Those images are not included in the repository or needed for the executable assertions. The actual application supplies its suite style; the standalone inspection did not qualify the full application layout.

The public API is declared in `include/tigersheets/financial_model_pane.hpp`. Editable definitions exclude generated cache/result/freshness data; context copies only sheet identities and editable configuration. Comparison `outputs` is its authored list of source result addresses/captions, not a generated output-cell cache.

# Integrated workbook acceptance

`acceptance.log` runs all seven workflow families through `executeFinancialWorkflow` and `WorkbookSession`: immutable preparation, explicit apply, scenario application Undo, forecast complete Undo/Redo, isolated comparisons, one/two-axis sensitivity, saved and applied Goal Seek/Solver with Undo, circular financing convergence, two edited XLSX save/reopen cycles and reopened generated-cell protection. `acceptance-build.log` records the private test link. This is a characterization/acceptance test against already-tested production behaviors, not an additional production RED/GREEN cycle.

`acceptance-independent.json` and `.log` record a separate read-only ZIP/XML/openpyxl inspector. Each of two files has 11 saved definitions across all seven families, 23 independent numeric output checks and 16 ordinary worksheet formulas. All XML parses, metadata fingerprints, custom sheet identities, typed source values and native number formats/iteration settings pass. Source arithmetic and scenario/sensitivity expectations are independent of TigerSheets calculation APIs. Authored formulas are uncached and require recalculation by a consuming application. Microsoft Excel itself was not run.

Reproduce after the parent-coordinated build:

```sh
QT_QPA_PLATFORM=offscreen ./build-release/financial_model_pane_tests
TIGERSHEETS_FINANCIAL_FIXTURE_DIR="$PWD/docs/validation/forecast-fixtures" ./build-release/financial_acceptance_tests
python3 tests/financial_acceptance_inspector.py docs/validation/forecast-fixtures/financial-workflows.xlsx docs/validation/forecast-fixtures/financial-workflows-resaved.xlsx --output docs/validation/forecast-tdd/acceptance-independent.json
```

See `ui-identities.json` for source/private-binary/log hashes at the focused pane freeze. Parent-coordinated full application/worker integration and broad CTest results are recorded separately for the milestone.

# Actual desktop and native worker

`desktop-acceptance-red.log` reproduces a qualified-preview state defect using the real MainWindow and native report worker: opening a different financial family hid the existing candidate's review controls. The host now refuses family changes while a financial job or workbook preview is active; opening the current pane retains its locked review state. `desktop-acceptance-green.log` verifies that fix and completes forecast preview→accept→one Undo/Redo, selecting a saved setup, source-edit stale status and refresh, scenario setup/comparison, sensitivity, empty-argument circular example (two new sheets only after acceptance), and id-only report deletion/discard. The live source remains unchanged before acceptance.

Historical local inspection of the actual 1400×950 application with a 440-pixel financial pane followed explicit recalculation and viewport repaint. It confirmed the application's suite styling, formatted forecast/comparison/sensitivity values, saved stale indicator, source-consent checkbox, scrolling configuration and persistent action controls. The local image is not included in the repository or required by the acceptance assertions. This was offscreen Qt rendering, not a Wayland qualification. `desktop-identities.json` binds the private desktop executable, its copied matched TigerSheets static archives, the real native worker, test source and recorded hashes. Later parent-coordinated final build records supersede these private focused-build identities.

```sh
QT_QPA_PLATFORM=offscreen ./build-release/financial_desktop_acceptance_tests "$PWD/build-release/tigersheets-report-worker"
```
