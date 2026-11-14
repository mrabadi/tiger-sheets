# Session and native file I/O TDD evidence

The requested tdd:tdd skill governs these changes. No Superpowers workflow is used.

Executed commands use g++ -std=c++20 -Iinclude, the relevant test and source files,
and -lzip -lpugixml for OOXML. Tests run from /tmp/tigersheets-*-tests.

- Session creation/cell write first failed because the deliberately new public session header did not exist; adding WorkbookSession produced a passing executable.
- Invalid-address batch failed with `invalid row must fail`; address/schema validation made the same test pass without partial changes.
- Undo/redo and isolated preview/accept first failed on missing public method symbols; implementation passed checks of revision monotonicity, preview isolation, one Undo and discard.
- Sheet creation/rename/style/clear failed with `sheet and style operations`; the real operations then passed, including duplicate-name rejection and formatting retained after clear.
- Metadata update failed with `chart metadata transaction`; a whitelisted set_metadata operation passed, and cannot forge workbook revision.
- CSV quoted/multiline roundtrip first failed on the missing public header. The parser/writer passed quoted commas, doubled quotes, CRLF, multiline cells and empty trailing fields.
- CSV file import/export failed on missing public methods; file I/O plus BOM support passed, including unterminated-quote rejection.
- OOXML initially failed on the missing public header. A native OPC writer/reader passed string, numeric and formula save/reopen without LibreOffice.
- Untouched imported copies failed `unchanged package copy is byte identical`; source-package retention passed byte identity and subsequent edited save preserving unrelated custom XML and VBA binary bytes.
- External modification failed `external content changes block save`; SHA-256 source comparison plus temporary-file fsync and atomic rename then passed, retaining external bytes and the prior baseline on failure.
- Native styles failed `native stylesheet exists`; native fonts/fills/number formats and cellXfs then passed bold/fill/style save/reopen.

The VBA binary test verifies preservation only. It does not execute, rewrite or establish validity of a macro project. Formula cached results are not synthesized by the OOXML mapper; Excel is asked to recalculate. ZIP members have explicit per-member/aggregate bounds. Unknown package members are retained on edited source-package saves; cell-value changes patch the corresponding XML rather than reconstructing all package parts.

Additional executed cycles:

- Chart persistence first failed `native chart part exists`; actual DrawingML chart/drawing/relationship parts then passed. An independent openpyxl load recognized the emitted chart as LineChart.
- CMake integration caught a real temporary-JSON iteration use-after-free while importing optional metadata. ASan on the linked archive identified it; a named JSON lifetime fixed it. Fresh archive and tests passed.
- Corrupt optional metadata failed with a JSON parse error; it now leaves the readable workbook and original package intact.
- Shared formulas failed `shared formulas expand relative references`; the importer now expands relative followers through the real reference transformer, and edited masters preserve follower formulas on save.
- Source worksheet deletion failed `deleted worksheets stay deleted after source save`; removing its workbook entry and relationship made reopen preserve the deletion.
- Authored VBA source failed `unimplemented native VBA authoring must refuse rather than save private source`; it is now rejected before creating a destination. Detached semantic snapshots of original macro workbooks likewise require the original package, preventing accidental macro loss.
- XLSM content type failed `XLSM file uses macro-enabled workbook content type`; it now emits the native macro-enabled main type and refuses preserving a macro project under .xlsx.
- Literal text failed `apostrophe input escape is not part of native text`; one shared native-cell writer now strips user escape syntax, restores text intent on import, and emits actual Boolean cells.

Final fresh isolated CMake ASan+UBSan build: `/tmp/tigersheets-io-asan`;
`ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/tigersheets-io-asan -R '^(session|csv|ooxml)_tests$' --output-on-failure`: **3/3 passed**, zero failures. Leak checking is disabled because LeakSanitizer reports an environment ptrace limitation; address/undefined-behavior instrumentation remains enabled. Source is frozen after this check.

Recovery-package regression: clearing a restored package's sourcePath/sourceHash while retaining its ZIP entries/baseline reproduced `Cannot open workbook: ` on an unchanged save. The exact-copy shortcut now requires a nonempty sourcePath; a detached package is rebuilt from its retained entries. The same OOXML executable passed with AddressSanitizer and UBSan after the fix (`ASAN_OPTIONS=detect_leaks=0 /tmp/tigersheets-io-asan/ooxml_tests`).

Structural-preservation regressions reproduced stale native references after worksheet rename: an imported chart without valid app editing metadata, worksheet-scoped print-area names, native tables, pivot caches, external links, and worksheet validation formulas. Those cases now reject structural save before creating the destination file, with a specific preservation diagnostic. The source package remains intact. Simple cell/formula edits still preserve these opaque parts. App-owned chart parts remain editable; prior app-owned drawing anchors and unused chart relationships are removed before current charts are rendered, so deleting an authored chart no longer leaves its native anchor behind. That deletion regression failed before the cleanup and passed afterward under ASan+UBSan.

An unchanged Save As regression appended outside bytes to the source after opening it. It failed `unchanged Save As cannot copy externally changed source bytes`; the byte-copy shortcut now verifies its sourceHash before writing a destination. This complements the existing conflict check around atomic saves to the source path.

These guards are conservative: table/pivot/external-reference parts and worksheet-scoped names currently prevent worksheet structural saves, rather than claiming unsupported reference rewriting. Some unused native chart parts can remain as preserved orphan parts after chart deletion; an ensuing structural save may therefore require preserving worksheet identity. Full imported-object editing, complete styles, native pivot serialization and MS-OVBA authoring remain separate compatibility gates.

Final fresh related check after all IO changes: `cmake --build /tmp/tigersheets-io-asan --target ooxml_tests session_tests csv_tests -j2` followed by `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/tigersheets-io-asan -R '^(session|csv|ooxml)_tests$' --output-on-failure` passed 3/3 tests in 0.43 seconds, with address and undefined-behavior instrumentation enabled. LeakSanitizer is disabled because the execution environment does not permit its ptrace workflow.

## Authored XML input safety

A final persistence regression embedded NUL in cell input and attempted to overwrite an existing workbook. RED: `embedded NUL must fail clearly before changing an existing workbook`. The writer now preflights application-authored XML strings before generating package changes or creating a temporary destination. Unsupported XML C0 controls produce a field/cell diagnostic, and rejection preserves both target bytes and the source package hash. Representative forbidden controls 0, 1, 8, 11, 12, 14 and 31 are covered.

A valid-whitespace test then failed `valid XML tab newline and carriage-return text must roundtrip exactly`. The prior XML serialization left literal CR characters, which an XML reader normalizes. Serialization now emits CR numeric character references outside CDATA/comment/processing-instruction blocks. Tab, LF, CR and CRLF text round-trips exactly; opaque retained ZIP entries are not scanned or rewritten by this guard.

The metadata/Unicode regression reported sixteen unprotected cases before the shared validator: fontFamily, numberFormat, alignment, defined-name keys/formulas, chart IDs/titles, calculation mode, malformed/truncated/overlong UTF-8, surrogate values, values above U+10FFFF, XML-forbidden U+FFFE/U+FFFF and malformed worksheet-name UTF-8. These cases now reject before target mutation. The small shared UTF-8 scalar decoder enforces XML 1.0 character ranges, while legal whitespace, accented Latin, Greek, CJK and supplementary-plane emoji remain supported. It validates known strings actually authored into XML; it does not recursively restrict opaque retained XML or JSON-only extension data.

The final RGB regression reported six unprotected exports: fill/color set to `red`, `#GG9900` or `#12345`. The writer now requires six RGB or eight ARGB hexadecimal digits, with an optional leading `#`, before save. Mixed-case six-digit RGB and eight-digit ARGB remain supported. Raw RED output is retained in `docs/validation/ooxml-rgb-red.log`.

This is bounded persistence protection, not complete OOXML schema, Excel identifier, formula or metadata semantic validation. Importer restructuring and opaque-extension validation were deliberately outside scope. Native Microsoft Excel behavior remains separately unverified.

Final focused GREEN command: `cmake --build /tmp/tigersheets-io-asan --target ooxml_tests -j2` followed by `ASAN_OPTIONS=detect_leaks=0 TIGERSHEETS_KEEP_FIXTURES=1 /tmp/tigersheets-io-asan/ooxml_tests`. It passed with `TIGERSHEETS_SANITIZE:BOOL=ON`; full raw output is `docs/validation/ooxml-input-safety-green.log`. No full build, GUI campaign or fuzz campaign was run for this bounded fix. Independent read-only openpyxl 3.1.5 checks also passed on the retained fixtures for exact tab/LF/CR/CRLF, accented/Greek/CJK/emoji text, Unicode defined names, Euro format text, and emitted native RGB/ARGB colors; raw result is `docs/validation/ooxml-independent-text-color.log`.
