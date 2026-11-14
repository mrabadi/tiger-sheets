# Merged-cell clipboard and sort guards

`src/grid.cpp` now protects merged layout while full merged-cell clipboard authoring remains unavailable. `tests/worksheet_merge_edit_guard_tests.cpp` covers the public model behavior.

- `merged-cut-red.log` reproduces a Cut move that strands the original span. `merged-cut-green.log` verifies refusal leaves source, destination, merge definitions and Undo unchanged, with an Unmerge-first explanation. Preparing Cut itself remains nonmutating.
- `merged-copy-red.log` detects missing merged-source clipboard information. `merged-copy-green.log` verifies complete and partial merged sources carry `sourceHasMerges`; ordinary, formula and transpose pastes refuse. Paste Values and Formats remain available to safe destinations, with one Undo.
- `merged-destination-red.log` reproduces multi-cell writes into a merged destination. `merged-destination-green.log` verifies value/formula/transpose and Cut guards, hidden-interior refusal, safe single-cell upper-left-anchor paste/move, format-only safety and Undo.
- `merged-sort-red.log` reproduces sorting complete or partial merged spans without moving the spans. `merged-sort-green.log` verifies Unmerge-first refusal plus ordinary unrelated copy, Cut, sorting and Undo.
- `merged-preflight-refactor-green.log` verifies the same guard suite after replacing per-cell merge searches with one bounded destination rectangle preflight. Multi-cell source rectangles that cross merged cells are refused conservatively; Formats bypasses this value guard. A single-cell anchor exception requires an actual 1-by-1 clipboard rectangle.

The existing `grid_tests` and `worksheet_correctness_tests` also pass against the directly compiled current grid object, both before and after the preflight refactor. The latter retains real calculation checks for typed Paste Values, calculated-key sorting, formula rebasing, atomic dependent-reference Cut moves and chart-source refusal. The ordinary grid executable is silent on success; its observed process exit code was zero.

All builds and runs were private `/tmp` executables using existing optimized native libraries and Qt's offscreen platform. Shared Ninja, final application integration and package validation remain owned by the parent task.
