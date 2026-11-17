# Otis standalone exercise: results

The optional local artifact `output/otis-90-minute/Otis-TigerSheets-Corrected.zip` contains the model and `Otis-10-K-Raw.xls`, plus compact results, disclosed fixes and hashes. Generated packages are not part of the Git repository. Keep the two workbooks together so external links remain portable. The [retained results summary](validation/otis-90-minute/RESULTS.json) records the completed work.

All three requested follow-up areas are complete: native UI behavior, workbook formatting and audit consistency.

## Completed validation before cleanup

| Check | Recorded result |
|---|---|
| Standalone video exercise | 9,952 top-level / 10,514 recursive actions completed. Full replication is outside built-in tests. |
| Native regression tests | 279 initial passes; the generic driver exceeded its 300-second limit, then passed unchanged serially in 240.17 seconds with a 900-second limit. All 280 tests have passing evidence. |
| Standalone Python checks | 273 passed, zero failures/errors/skips. All 9,093 source and 1,089 native/binary input hashes stayed unchanged during that run. |
| Formatting repair | 125 top-level / 147 recursive native UI actions corrected column B and 59 approved style cells. Residual saved-file failures went from 65 to zero. All 891 stored cell contents/types and 611 formulas/caches stayed unchanged. |
| Saved and relocated pair | Four file audits passed. Both native reopen/refresh phases passed 41 top-level / 44 recursive actions, with 161 cell guards at each of eight checkpoints. Raw source, external caches and link bindings passed. |

The full 273-Python and 280-native validations above were completed before cleanup; they were not rerun afterward. The original timeout remains part of the recorded native outcome.

## Fixes

F12 opens Save As; fresh `=` shows the SUM selector; AutoSum uses genuine Point mode with arrows, mouse selection and F4. Short bold formulas stay on one line when space permits. Long selections retain their scrolled viewport and active anchor, with floating dimensions when the anchor is offscreen. Quick Analysis follows an eligible completed range while scrolling.

Increase Indent now gives General-aligned literal text explicit Left alignment. A disclosed native UI repair corrected source-visible width, indentation, boldness, colors, Accounting symbols and blank-cell styles. Source-first audit rules consistently honor the exact source-entered percent code, the source literal override and source-visible E37 black text. Other format, precision and whitespace mismatches remain subject to strict checks.

The native implementation remains in [worksheet interaction](../src/worksheet_view.cpp), [desktop commands](../src/main_window.cpp), [indent shortcuts](../src/tutorial_shortcuts.cpp) and the [audit adapter](../tools/audit_otis_native_final.py). Narrow regression tests remain in the source tree; full tutorial replication is a standalone exercise.

## Public source and local exercise data

The public checkout includes [compact BIFF regression fixtures](../tests/fixtures/biff/README.md) and the retained results summary. Product tests use these fixtures without the local validation corpus. The full exercise runners and audit tools remain standalone; their original video/PDF inputs, detailed source ledgers, screenshots, generated manifests and workbook outputs are local data and are excluded from Git. They are not prerequisites for the built-in product test suite.

## Retention and limits

Generated replay trees, screenshots, logs, raw audit inventories, failed-run copies and obsolete packages were removed at the user's request. The corrected pair, compact results, source/tests and necessary original input fixtures remain. Old raw evidence is no longer locally available; this document links only retained material. Installers exclude `docs/validation` rather than shipping generated validation archives.

The original exercise's four 239-check failures are historical outcomes and are not relabeled as passes. Corrections were explicit post-exercise work followed by new save/reopen/refresh checks. No fresh full-video rerun or exhaustive pixel comparison was performed. Helvetica display is accepted; `sourceReviewComplete` and `verifiedReplica` remain false. Complete Excel/VBA compatibility, actual Microsoft Excel interoperability and clean-machine acceptance are separate work.
