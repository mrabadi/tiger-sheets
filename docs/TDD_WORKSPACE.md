# Workspace and modeling TDD evidence

The complete release remains gated by the frozen compatibility catalogs. These records cover individual implemented behaviors, not Excel parity.

## References

`g++ -std=c++20 -Iinclude tests/references_tests.cpp src/references.cpp -o /tmp/tigersheets-reference-tests && /tmp/tigersheets-reference-tests`

- RED: new public reference API was absent. GREEN: mixed relative/absolute copy references passed.
- RED: copying changed a string, sheet token, and LOG10 function name. GREEN: token boundaries and quoted literals are respected.
- RED: copying A1 beyond the worksheet produced an invalid address. GREEN: the result is `#REF!`.

## Modeling

`g++ -std=c++20 -Iinclude tests/analysis_tests.cpp src/analysis.cpp -o /tmp/tiger-analysis-tests && /tmp/tiger-analysis-tests`

- RED: Goal Seek API absent. GREEN: revenue less cost reaches the requested profit without mutating inputs.
- RED: non-finite model output was not diagnosed. GREEN: `invalid_result` is explicit.
- RED: sensitivity-table API absent. GREEN: every two-variable combination is evaluated.
- RED: pivot API absent. GREEN: grouped revenue totals and grand total match input records.

## Native grid

`cmake --build build --target grid_tests -j2 && QT_QPA_PLATFORM=offscreen build/grid_tests`

- RED: worksheet model API absent. GREEN: Excel dimensions and edits through the canonical session passed.
- RED: calculated-value API absent. GREEN: displayed values remain separate from editable formula source.
- RED: clipboard API absent. Implementation uses one canonical transaction and parsed-reference shifting.

## Workspace

`cmake --build build --target workspace_tests -j2 && QT_QPA_PLATFORM=offscreen build/workspace_tests`

- RED: window API absent. GREEN: native grid, Home ribbon, and formula bar commit to the active cell.
- RED: calculation entry point absent. Actual IronCalc integration is being verified separately.

## Release gate

`python3 tests/release_gate_tests.py`

- RED: gate API absent. GREEN: missing/failed required formula evidence blocks release.
- RED: a bare passing Boolean bypassed the gate. GREEN: test and executed-case references are required.
- The current complete-release gate correctly fails for uncovered catalog entries.
