# Native workbook authoring tools

The financial-model request exposed an incomplete tool contract: the model could discover operation names, but no cell arguments, range-writing interface or native chart schema. Sequential preview requests also replaced earlier proposed data. `WorkbookTools` now owns the built-in, revision-checked workbook tool bridge, including a complete financial-model example and explicit operation schemas. A later chart request extends the pending data proposal. Acceptance and saving remain desktop actions.

The native tests compile independently against the existing core/reference libraries:

```sh
g++ -std=c++20 -Iinclude tests/workbook_tools_tests.cpp src/workbook_tools.cpp build/libtigersheets_core.a build/libtigersheets_references.a -o /tmp/tiger-workbook-tools-tests
/tmp/tiger-workbook-tools-tests
```

Observed RED/GREEN cycles:

1. A rectangular financial range test failed to compile because `workbook_tools.hpp` and its implementation were missing. The minimum bridge then passed isolated preview creation, numeric/boolean/null conversion and formula-string retention.
2. The sequential financial data/chart test failed with `a later chart request extends the financial preview`. Accumulating native operations at the same live revision then passed branch reads, existing-chart retention, user acceptance and one-step undo of the combined model/chart.
3. Capability discovery failed with `the built-in workbook contract is discoverable`. Publishing four native tools, eight operation schemas, stable sheet identities, formula capabilities and a complete financial-model/chart example made it pass.
4. Malformed range requests failed with `invalid matrix types, dimensions, coordinates and aggregate limits must reject`. Strict integer/bounds, rectangularity, finite-number and aggregate-operation checks then passed while retaining the previous valid preview on every rejected request.
5. Chart validation failed with `new charts require unique ids, native families, real sheets, rectangular data and bounded geometry`. Native chart validation then passed missing-field, duplicate-id, unknown-sheet/family and invalid geometry cases; default anchors are materialized for consistent rendering and persistence.
6. Native operation validation failed with `native tool validation rejects narrowing, unadvertised operations, oversized edits, duplicate charts and macro enablement`. Argument validation, metadata chart validation and the desktop-only macro enablement rule made it pass. Source proposals retain already user-enabled macro settings and project metadata.
7. Pending branch search failed because its response lacked `view`. Implementing bounded reads/searches and metadata inspection made it pass. Oversized metadata is explicitly omitted and cannot be destructively replaced.
8. A calculated preview read failed to compile because `WorkbookTools::calculate` was missing. Adding the host calculation callback made it pass: read cells retain raw formulas alongside matching-revision values; failed/stale calculation states remain explicit.

After these GREEN cycles, a characterization check confirmed that acceptance/discard clears proposal history and concurrent live changes produce a conflict before a fresh branch is prepared. No production change was needed for that final check. The standalone native test executable reports `workbook tools tests passed` for all nine groups.

The published `schemas/workbook.v1.json` is generated from the actual registered definitions. Formula evaluation is supplied by the desktop's existing engine; the bridge neither commits changes nor opens/saves files or enables macros. End-to-end fake-process, GUI, XLSX and installer validation is recorded separately by the application integration tests.
