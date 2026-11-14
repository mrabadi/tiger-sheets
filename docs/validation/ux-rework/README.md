# Independent native interaction review

Date: 2026-11-11. Executables were compiled and linked separately under `/tmp` against the current application libraries. No shared build, CMake configuration, or production source was changed by this review. `ninja -t commands workspace_tests` was used only to read link flags.

## Confirmed failures before the root fixes

- Selecting a second chart left the first chart's selection handles visible. `chart_interaction_tests selection` failed its exclusive-selection assertion. The first observed output is retained in `chart-selection-red.log`.
- Clicking a worksheet cell left chart selection handles visible while keyboard commands now targeted the cells. This makes Delete's target unclear and can clear cell data while a chart still appears selected. `chart_interaction_tests grid` failed its deselection assertion. The first observed output is retained in `chart-grid-deselect-red.log`.
- During an active readonly Codex chart preview, select E5 in the financial example, focus its formula field, press F4, then click A1. The main window's programmatic reference rewrite changed `=D5*(1+$B$5)` into `=$D$5*(1+$B$5)` despite `isReadOnly()==true`, set the modified flag, and the next focus change threw `Cannot accept the current cell edit` out of the editing-finished callback. The isolated process aborted with SIGABRT (Python return code -6; shell exit 134). `readonly-f4-crash-red.log` is the directly captured raw subprocess output. `readonly-f4-assertion-red.log` captures the safer assertion failure before the focus-out crash. The preliminary CLI name `preview-shortcut` in that log was renamed `readonly` in the final test source.

The first two RED outputs were observed in the initial tool runs and then transcribed into their labeled logs after the root's updated library was already available. Their GREEN logs are direct subsequent subprocess captures. The crash and readonly assertion logs are direct captures from the independently linked pre-fix executables.

After the F4 guard was linked, the root executed the final readonly test and observed a second RED: Insert Function still opened a writable dialog. The root's raw `/tmp/tigersheets-ux-readonly-insert-red.log` is preserved here as `readonly-insert-function-red.log`; the root then added a readonly command guard. This is separate from the F4 assertion and crash evidence.

## Coverage and handoff

The final `tests/chart_interaction_tests.cpp` has individually runnable groups: `selection`, `grid`, `cancel`, `resize`, `edit`, `preview`, and `readonly`. The readonly case also checks that Insert Function cannot open a writable dialog or alter a readonly formula, with a zero-delay cancellation timer to keep a regression deterministic.

Before the readonly guards landed, cancellation, live resize/Escape, released resize/undo, combined type/range edit plus XLSX save/reopen/undo, and preview chart deletion/edit/movement protection all passed. The root's exclusive-selection and grid-deselect changes then passed independent relinking; raw outputs are in the two GREEN logs.

Final development GREEN: the root's complete current development CTest run passed **37/37 tests**, including `chart_interaction_tests` with all seven groups and both readonly guards. The genuine raw `/tmp/tigersheets-ux-development-ctest.log` was read, its complete success line verified, and preserved as `ctest-development-final.log` here. This verifies the chart selection fixes, readonly formula protections, and narrow native ribbon integration alongside the broader development suite. Release CTests and the isolated full-main-window ASan run were still in progress at this documentation handoff; neither is claimed as verified here.

These checks validate the specific integrated interactions above. They do not qualify full Excel chart or workbook compatibility.

## Historical local visual inspection

A separate temporary MainWindow probe opened the real financial example at 820×650, processed events, waited 50ms, and inspected the settled window before traversing the ribbon. It measured an 86px ribbon (72px plus the native 14px scrollbar), overflow enabled, native scrollbar visible, natural content width 1093px, and 273px horizontal range. Tab then reached Fill Down and scrolled it fully into view. Raw measurements are in `ribbon-integration-green.log`.

The same probe widened the window to 1380×900 and inspected chart selection and the actual native chart dialog. The local renders showed readable font/alignment controls and the native ribbon scrollbar; right-hand chart content naturally extends beyond the narrow worksheet viewport. These historical images are not included in the repository or required by the interaction assertions.

An additional CLI-geometry probe omitted explicit resize and passed `-qwindowgeometry 820x650+0+0`. Immediately after the first `processEvents()`, overflow was enabled and the ribbon was already 86px, but the native scrollbar still reported invisible. After 50ms it was visible and Tab navigation passed. This reproduces delayed scrollbar visibility during an early CLI observation, not persistent missing overflow or a persistent 72px ribbon. The settled probe above verified the resulting geometry.


## Final root validation

The final optimized build passes 37/37 CTest targets (`tigersheets-ux-release-final-ctest.log`, `ctest-release.xml`). Six focused native C++ suites pass ASan/UBSan (`tigersheets-ux-asan-final-ctest.log`, `ctest-asan.xml`) after the reproduced observer-lifetime fix. Leak detection is disabled and no full-release claim follows from these checks. The installed demo launches from an isolated temporary prefix, and independent financial checks pass 13/13. `summary.json` retains exact source/binary/record hashes and the unchanged blocked release gate.
