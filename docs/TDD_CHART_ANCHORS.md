# Chart pixel offsets in native workbook anchors

Chart placement now retains `anchor.offsetX` and `anchor.offsetY` as optional nonnegative integer pixel offsets within the anchored cell. `WorkbookSession` accepts 0 through 1,000,000 inclusive and rejects negative, oversized, fractional, string, boolean, null and overflowing integer values before committing a transaction or preview. A failed mixed cell/metadata transaction preserves the workbook, revision and undo state. Imported semantic snapshots use the same validation.

The OOXML writer encodes offsets in the real DrawingML `oneCellAnchor/from/colOff` and `rowOff` fields using 9,525 EMU per pixel, with 64-bit multiplication. Existing 1-based semantic row/column anchors still become 0-based DrawingML row/column indices. Absent offsets write zero. Native zero offsets and the maximum supported offset (9,525,000,000 EMU) are tested.

The importer reads native offsets for charts already mapped through valid TigerSheets metadata, the worksheet's drawing relationship, and the `TigerSheets:<chart-id>` graphic-frame name. It reads native offset values independently of private offset fields, including explicit zero. Fractional-pixel EMU values round to the nearest pixel; malformed or out-of-range mapped offsets produce a diagnostic. This does not add third-party chart source/type/range mapping or change opaque drawing preservation. The existing mapping policy for missing/stale/corrupt optional metadata remains unchanged.

## TDD evidence

All focused commands used the isolated existing sanitizer build `/tmp/tigersheets-io-asan`; no shared CMake, MainWindow, chart widget, worker, or grid file was edited.

1. Session RED: `chartPixelOffsetsAreValidatedAtomically` failed with “malformed chart pixel offsets must reject the entire transaction”. Minimal optional-field validation then made the same session target pass.
2. Native writer RED: inspecting real DrawingML failed with “chart pixel offsets must be encoded into real DrawingML EMU anchors” because both fields still contained zero. Writing `offsetX/offsetY * 9525` as 64-bit values made the same OOXML target pass, including save/reopen, explicit zero and the >32-bit EMU boundary.
3. Native reader RED: a fixture retained valid native drawings but removed private offset keys. It failed with “mapped native DrawingML offsets import independently of private offset fields”. Hydrating offsets from the mapped native `oneCellAnchor` made the same target pass.
4. Verification also confirms invalid-offset save attempts preserve existing package bytes and source-baseline hash.

Build commands: `cmake --build /tmp/tigersheets-io-asan --target session_tests -j2` and the analogous `ooxml_tests` target, followed by each binary with `ASAN_OPTIONS=detect_leaks=0`. Both complete focused test binaries passed again after the final source change. AddressSanitizer and UndefinedBehaviorSanitizer were enabled by the existing target configuration; leak detection was disabled for the environment limitation.

Durable raw RED/GREEN and final sanitizer outputs are in `docs/validation/chart-anchor-offsets-tdd.log`. Final retained fixtures are `/tmp/tigersheets-ooxml-iiZEWi`.

## Independent native-reader check

Openpyxl 3.1.5 independently opened the normal offset fixture, the fixture without private offset keys, and the large-offset fixture. It read real `OneCellAnchor` coordinates `(column=5,row=1)` with EMU offsets `(257175,180975)`, `(257175,180975)`, and `(0,9525000000)` respectively. The report is `docs/validation/chart-anchor-offsets-independent.json`. Microsoft Excel was not run. Host grid-to-cell conversion, runtime placement and application undo/redo are checked by the parent integration tests.
