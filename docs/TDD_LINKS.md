# Local workbook links

`prepareWorkbookLinks` discovers native Excel external-workbook relationships
and prepares a transient snapshot for the actual calculation engine. It does
not change the canonical workbook or write any source file.

## Host API

```cpp
WorkbookLinkOptions refresh;
refresh.refresh = true;                    // Explicit user refresh action.
auto prepared = prepareWorkbookLinks(currentWorkbook, &originalPackage, refresh);
auto retained = prepared.snapshot["links"]; // Keep only in the open workbook's state.
auto values = engine.calculate(prepared.snapshot);

WorkbookLinkOptions recalculate;
recalculate.retainedLinks = retained;
prepared = prepareWorkbookLinks(editedWorkbook, &originalPackage, recalculate);
values = engine.calculate(prepared.snapshot); // No source-file reads.
```

`WorkbookLinkResolution` returns `snapshot`, descriptor-only `links`,
`diagnostics`, `filesRead` and `bytesRead`. The counters describe successfully
decoded source packages and their file sizes; failed attempts are also charged
against internal read budgets. `result.links` removes loaded source workbook
bodies and preserves original paths for display. Keep `result.snapshot["links"]`
in memory for later recalculation. Preserve canonical cells and link paths for
editing/saving; do not save normalized transient formulas or loaded sources.

The default options never read a linked source file. They use a matching
previous refresh supplied in `retainedLinks`, or saved native external-link
cache values. A disabled canonical link cannot reuse retained live sources.
Retained matching requires the same original path and workbook name. Clear
retained state when closing/replacing the workbook and when deliberately
discarding a refresh. Changed source files become visible on the next explicit
refresh.

`relocatedPaths` maps an original descriptor `path` to a user-selected local
path. Relative targets are resolved against the owning package's directory;
without a package they are relative to the process working directory. Local
`file://` URIs and percent-escaped relative relationship URIs are decoded.
HTTP/other URI schemes, UNC targets and remote file-URI hosts are refused.
Relocation can explicitly replace such an old target with a local file.

Defaults limit refresh to 16 source-file attempts, depth 4, 64 MiB of total
source file sizes, and 128 MiB of declared expanded ZIP data. ZIP metadata is
checked before workbook decoding. The existing OOXML reader additionally limits
package members and expanded sizes. Active-path cycles stop before rereading an
active workbook. Repeated aliases reuse an already prepared source and retain
their distinct Excel external-reference indexes.

## Native Excel metadata

Discovery follows the ordered `externalReferences` in `xl/workbook.xml`, their
workbook relationships, each externalLink part, and its externalBook path
relationship. `[1]Sheet!A1` indexes use reference order, rather than assuming
relationship IDs are numeric. Index normalization recognizes sheet references
outside Excel string literals and runs on copies of current and linked inputs.

Saved `externalBook/sheetNames` and `sheetDataSet` values become explicit literal
cached workbooks. Supported cache types are numeric, boolean, string and known
worksheet error values. Formula-looking strings stay literal. Invalid values
and unsupported types produce diagnostics instead of coercion or execution.
The implementation uses zero-based external cache sheet IDs, as shown in
Microsoft's native package example in the [SpreadsheetML
primer](https://download.microsoft.com/download/e/1/4/e14fb96f-83b8-4a2a-84db-7fa8acbe061a/Office%20Open%20XML%20Part%203%20-%20Primer.pdf).
Microsoft documents the [external sheet data
cache](https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.spreadsheet.externalsheetdata)
and [external workbook element](https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.linq.x.externalbook).

Link states include `refreshed`, `retained`, `cached`, `unavailable`, `missing`,
`invalid`, `blocked`, `limited` and `cycle`. Diagnostics distinguish cached use,
missing files, invalid packages/metadata, nonlocal targets, invalid URIs, budget
limits and cycles. A failed refresh can retain an existing saved literal cache;
the failure remains visible in diagnostics. Without usable source data, the
calculation engine returns reference errors.

## Verified TDD cycles

Each behavior below was tested before its production implementation. Focused
tests write isolated XLSX fixtures under `/tmp`, then use the actual OOXML reader
and actual calculation engine.

| Behavior | Observed RED |
| --- | --- |
| Explicit local refresh and `[1]` normalization | Undefined new `prepareWorkbookLinks` symbol |
| Default cached values, no source reads | `Default link state must identify use of cached data` |
| Missing-source diagnostics | Exception `Cannot open workbook: .../Missing.xlsx` |
| Path relocation | `Explicit relocation must refresh the selected local source` |
| Network refusal | `Network targets must be blocked before source reads` |
| Local file URI escaping | `Local file URIs must decode escaped path characters` |
| File/byte/expanded/depth bounds | `Source read budgets must prevent workbook decoding` |
| Recursive dependencies | `Linked source workbooks must resolve their own declared local dependencies` |
| Cycles | `A workbook cycle must stop before rereading the original workbook` |
| Invalid metadata isolation | Exception `Invalid external workbook XML` |
| Shared source/index identity | `Calculated linked value was missing` |
| Escaped relative relationship names | `Relative relationship URIs must decode escaped file names` |
| Malformed cache values | `Malformed cached types must be diagnosed instead of coerced or executed` |
| Retained-source recalculation | Missing new `retainedLinks` option |

Observed GREEN: the complete focused suite passed after every cycle. It verifies
live formula input `3+4` produces root result 14, saved cache 3 produces 6,
recursive source dependencies produce 18, and edited formulas using retained
live data produce 21 with zero new source reads. Canonical formulas/metadata and
string literals remain unchanged. Final verification commands:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude \
  tests/workbook_links_tests.cpp src/workbook_links.cpp \
  build/libtigersheets_io.a build/libtigersheets_core.a \
  build/libtigersheets_references.a build/libtigersheets_calculation.a \
  rust/ironcalc_bridge/target/release/libtigersheets_calc_bridge.a \
  -lzip -lpugixml -ldl -lpthread -lm -o /tmp/tigersheets-links-tests
/tmp/tigersheets-links-tests
```

Final result: all 14 focused scenarios passed after the indentation-only
refactor. Compilation with `-Wall -Wextra -Wpedantic` produced no warnings.
The helper and test were also built with AddressSanitizer and
UndefinedBehaviorSanitizer; `ASAN_OPTIONS=detect_leaks=0` completed with
`workbook links tests passed` and no sanitizer reports. Linked existing I/O,
calculation and Rust archives were not rebuilt with these sanitizers.
LeakSanitizer is **unverified**: enabling it ran the tests successfully, then
terminated with its stated sandbox limitation, `LeakSanitizer does not work
under ptrace`.

The helper library needs the existing `tigersheets_io` target; its integration
test additionally needs `tigersheets_calculation`. Root CMake/UI integration is
owned by the parent task.

## Remaining limits

This helper does not rewrite native external-link caches on save, implement
external named ranges, DDE/OLE links, network refresh, cross-workbook iterative
cycles or every Excel reference form. The calculation engine still requires
unambiguous linked workbook names and has its separately documented formula,
name and date-system limits. Existing source caches may be incomplete or stale;
using them is explicit. The helper is synchronous and should run on the host's
calculation worker. Tests do not establish complete Excel link conformance.
