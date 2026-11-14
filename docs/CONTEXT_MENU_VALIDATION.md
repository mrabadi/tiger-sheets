# Worksheet context-menu development validation

Version **0.3.0~dev.2**, Ubuntu amd64, 2026-11-12. The complete Excel-compatible release gate remains **BLOCKED**. This maintenance iteration extends the forecasts/what-if milestone; it does not replace the earlier reporting or financial qualification records.

## Changes and focused evidence

The [user guide](CONTEXT_MENUS.md) describes cell, header and frozen-row menus. Implemented operations include merge/unmerge, row/column insertion and deletion, native dimensions and visibility, clipboard/Paste Special, formatting, notes and selected-range sorting. Shift+F10/Menu-key access and Ctrl+1 formatting use native Qt widgets. Workbook edits are revision checked and undoable.

The [TDD evidence index](validation/context-menu-tdd/README.md) retains observed RED/GREEN cycles for the missing menus, keyboard access, stale callbacks, structural reference changes, native layout/persistence, formatting, column units, merged-cell safety and CSV retention status. Actual dialog acceptance/cancellation, viewport painting, small dimensions, two width save/reopen cycles and whole-selection behavior are exercised. Existing ordinary clipboard/sort/session/OOXML tests remain part of regression coverage.

Native width expectations include Microsoft's independent **8 characters → raw width 8.7109375 → 61 pixels** example at digit width 7, rather than deriving expected values from the implementation. [Microsoft's SpreadsheetML column equations](https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.spreadsheet.column?view=openxml-3.0.1) define the conversion. The application uses its current normal cell font (Calibri 11 with the installed Qt font fallback); exact rendering against Excel's fonts and imported alternative Normal styles is not qualified.

Merge loss-of-value confirmation is cancellable. Partial merge collisions, unsafe merged-cell clipboard/sort operations, non-anchor writes, unsupported structural references and opaque imported objects refuse before publishing changes. Imported original hidden values and unrelated package parts remain preservable. Hiding a row recalculates SUBTOTAL correctly and marks indirectly dependent native reports stale; Undo/Redo restores both visibility and freshness.

Focused reference/layout sanitizer checks are recorded with their instrumentation limits. Screenshots of the actual menus and themed Format Cells dialog are retained beside the test logs. Qt tests run offscreen; the images establish layout/legibility, not native window-system qualification.

## Final build and delivery

The final optimized build passes **123/123 CTest targets**, zero failures, **37.32 seconds**. The final build includes all source changes, rebuilt reference dependents and the original reporting deadline assertions with isolated test scheduling. An immediate no-change build reports `ninja: no work to do`. The source manifest hashes **786** build/test/dependency inputs.

The optimized build and complete CTest run are recorded in `validation/context-menu-tdd/final-build.log`, `final-ctest.log` and `final-ctest.xml`. The final source manifest and delivery identity record are retained in that folder. The exact installer audit is delivered alongside `dist/tiger-sheets_0.3.0~dev.2_amd64.deb` as its `.validation.json`; `dist/SHA256SUMS` identifies the audited bytes. The audit is external to the installer to avoid embedding its own checksum.

The private-stage audit checks payload/version, icons/MIME/desktop identity, Qt plugins, relative ELF runtime paths, bundled solver libraries and real worker execution. APT upgrade simulation does not install anything. These checks use this Ubuntu host's installed runtime; no clean-machine installation or system upgrade is implied.

The completed external audit passes **21 checks** on the exact **81,485,604-byte** installer, SHA-256 `dfe52879617449fed95bd22a4809e66c569317e584296464bfb7f395f32f4363`. The staged application reports `0.3.0~dev.2`; actual packaged Solver execution succeeds. APT simulation proposes upgrading installed `0.3.0~dev.1` to `0.3.0~dev.2`, with one upgrade, no new packages and no removals. No installation was performed. This post-package audit and delivery record are external handoff artifacts.

## Remaining qualification

The [guide's limits](CONTEXT_MENUS.md#development-limits) apply: bounded formatting/clipboard/layout actions, at most 10,000 merged ranges, unsupported merged geometry transfers and conservative refusal of imported structural transformations. Native raw column widths remain capped at 255; the maximum displayed character count depends on the measured digit width. Tiny/sub-character width parity is not fully qualified.

Complete Excel menu/function/shortcut compatibility, advanced worksheet objects, native edited VBA export, actual Microsoft Excel reopen/VBA execution, full printing of the new layout combinations, and complete platform/accessibility qualification remain unverified or incomplete. No fresh X11/Wayland session, screen-reader, authenticated Codex financial workflow, reference-machine performance run or clean VM install was performed for this maintenance revision. Earlier [forecast](FORECAST_RELEASE_VALIDATION.md) and [reporting](RELEASE_VALIDATION.md) results retain their original boundaries; they are not presented as new measurements.
