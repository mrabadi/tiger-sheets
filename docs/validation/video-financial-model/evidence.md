# Fill handle and header auto-fit TDD evidence

These records preserve actual executed tool responses from this task. The original runs returned their output directly to the chat; no corresponding `/tmp` log files were retained. Each linked record identifies the exact command, observed exit status, verbatim output, and execution chunk. They are preserved transcripts, not reruns.

All test commands ran from `/workspace/tiger-sheets` with `QT_QPA_PLATFORM=offscreen`.

| Behavior | Verified RED | Verified GREEN |
| --- | --- | --- |
| Horizontal fill shifts relative columns, preserves absolute references and source formatting; one undo restores the range | [horizontal RED](fill-horizontal-red.log): `build-release/worksheet_fill_handle_tests` exited 1 at the missing-copy assertion | [horizontal GREEN](fill-horizontal-green.log): same command exited 0 |
| Vertical fill shifts relative rows and preserves mixed locks | [vertical RED](fill-vertical-red.log): `build-release/worksheet_fill_handle_tests vertical` exited 1 at the mixed-reference assertion | [vertical GREEN](fill-vertical-green.log): the complete fill suite, including the new vertical test, exited 0 |
| Escape cancels a preview without writing on release | [Escape RED](fill-escape-red.log): `build-release/worksheet_fill_handle_tests escape` exited 1 because release still wrote cells | [Escape GREEN](fill-escape-green.log): complete fill suite, including Escape, exited 0 |
| Holding a dragged handle at the viewport edge scrolls; release fills the newly visible endpoint | [edge-scroll RED](fill-edge-scroll-red.log): `build-release/worksheet_fill_handle_tests scroll` exited 1 at the missing-scroll assertion | [edge-scroll GREEN](fill-edge-scroll-green.log): same command exited 0 |
| Native header boundary double-click persists width through recalculation, native XLSX save/reopen, and undo | [auto-fit RED](autofit-persistence-red.log): `build-release/worksheet_autofit_ui_tests` exited 1 because width was only visual | [auto-fit GREEN](autofit-persistence-green.log): auto-fit followed by complete fill suite exited 0 |
| Auto-fit preserves legacy hidden columns lacking width entries | [hidden-columns RED](autofit-hidden-columns-red.log): `build-release/worksheet_autofit_ui_tests` exited 1 at the hidden-column assertion | [hidden-columns GREEN](autofit-hidden-columns-green.log): same command exited 0 |

The latest [complete fill run](fill-focused-final-green.log) also passed visible-handle/no-op-click checks and read-only-preview/merged-destination protections. Those are boundary checks of the implemented fill behavior; no separate missing-behavior RED is claimed for those checks.

Build commands used after the native environment was repaired:

```sh
cmake --build build-release --target worksheet_fill_handle_tests -j2
cmake --build build-release --target worksheet_autofit_ui_tests -j2
cmake --build build-release --target worksheet_autofit_ui_tests worksheet_fill_handle_tests -j2
```

The first horizontal RED executable was compiled with the existing Ninja-generated C++ compile/link commands while the cached Cargo executable was missing. Its executed test failure is retained above. Missing Cargo and later unrelated concurrent compilation failures were environment/build failures, not counted as valid RED phases. The repaired build uses an explicitly configured prebuilt calculation archive; these C++ tests do not verify Rust source compilation.

Repeated fill-state cleanup was consolidated in `cancelFill`, and pointer-to-target updates in `updateFillTarget`; the passing complete suites verify those small refactors. Final shared build and broader regression outcomes are recorded separately by the root task.
