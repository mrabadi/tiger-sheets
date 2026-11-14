# Final reporting desktop measurements

The final optimized release binaries passed all four stages in
[`final-summary.json`](final-summary.json). Executable hashes, commands,
25-ms sampled process-tree RSS and elapsed process times are retained there.
Application and both worker hashes are also recorded in
[`persistence-performance-final-identities.json`](../reporting-tdd/persistence-performance-final-identities.json).

The visible Qt offscreen fixture contains exactly 100,000 data records plus headers
in `A1:J100001`, ten fields and one source formula. Fresh and warm phases change
that formula; the warm phase first completes ordinary calculation outside the
report timer. Each report measurement includes capture, worker input preparation,
isolated aggregation, prepared publication and a forced visible viewport repaint.
It runs the actual Qt event loop without a polling sleep between capture batches.

| Final measurement | Result | Budget |
| --- | ---: | ---: |
| Initial report | 5,131 ms | 15,000 ms |
| Fresh refresh, including source calculation | 6,418 ms | 10,000 ms |
| Changed-source refresh from current calculated results | 2,278 ms | 3,000 ms |
| Unchanged current-source refresh | 2,008 ms | 3,000 ms |
| Longest observed report UI dispatch | 43.723 ms | 100 ms |
| Input-to-painted-feedback p95, ten edits and ten new-cell insertions | 22.048 ms | 50 ms |
| Sampled report process-tree peak RSS | 2,632,392 KiB (2.51 GiB) | 6 GiB |

Totals, one report Undo/Redo and unchanged revision/history checks all pass.
Final repaint took 7–12 ms. Current-source reuse is restricted to matching,
converged revision/context results without workbook links or native external-link
dependencies. Linked inputs retain fresh calculation and cached-source consent.
Progress/cancellation and forced worker shutdown have separate worker/capture
regression coverage; the dispatch figure above is not a cancellation measurement.

The separate original 100,000-cell workload, with 20,000 formulas across twenty
sheets, recalculated in 609.858 / 557.635 / 561.427 ms. The one-million-cell stress
workload contains 200,000 formulas and recalculated in 6,039.809 ms; the one-second
target applies to the original 100,000-cell workload. These fixtures differ from
the reporting source and do not establish arbitrary formula-workbook timings.

Host-specific hardware and system details are redacted from the public record.
These measurements do **not** qualify the specified reference configuration.
Chart-inclusive native save/reopen, independent package inspection, enabled-UDF
and linked-source fixtures, and durable recovery timings are recorded separately.
Those checks are not included in these report-action timings.

Large native-report Undo separately measured 126.438 ms, with Redo 35.537 ms.
Both retain the original million-cell source allocation, but the optional
100-ms Undo qualification remains **failed** because cache cleanup still costs
time. See [`persistence-native-history-quiet-final.log`](../reporting-tdd/persistence-native-history-quiet-final.log).

Earlier REDs and intermediate probes remain alongside the final records. The
initial 24,472-ms report probe used a polling driver that added delay between
capture batches; it must not be compared with the final time as a pure product
speedup. The separate initial 73.942-ms input p95 was a genuine insertion-path
regression. The indexed insertion fix produced 22.913 ms in its focused GREEN
and 22.048 ms in the final optimized rerun.
