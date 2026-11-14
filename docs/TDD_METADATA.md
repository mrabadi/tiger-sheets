# Canonical metadata validation

`WorkbookSession` validates chart metadata before committing a transaction or
accepting a preview candidate. Validation also runs when constructing a session
from imported/recovered JSON. Rejected batches preserve cells, revision and
undo history. Existing minimal chart records such as `{id,type}` remain valid.

Provided chart fields are checked as follows:

- `charts` is an array of objects.
- Optional `id`, `type`, `sheet` and `title` are strings.
- A supplied `range` is an object with integer `row1`, `row2`, `col1` and `col2`,
  inside worksheet bounds and in increasing order.
- Range differences are at most 10,000 rows and 100 columns, matching
  `chartData`'s 10,000 data rows / 100 series beyond the header/label cells.
- A supplied `anchor` is an object. Optional row/column fields are integers in
  worksheet bounds; optional width/height are integers from 1 to 8,192 pixels.
- Floating values, booleans, strings and overflowing unsigned integers cannot
  be used where an integer is required.

Unknown fields and chart type strings remain preserved. This is lightweight
validation of the application's consumed fields, not complete Excel chart
schema or rendering conformance.

VBA metadata, when supplied, must be an object. Optional `enabled`,
`projectPresent` and `sourceAvailable` fields must be booleans. Optional
`modules` must be an array of objects, with string `name`/`source` when present.
Opaque imported projects with disabled execution and an empty module array
remain valid. This validates container types; it does not parse VBA source,
enable macros or establish VBA conformance.

Cell styles validate the fields consumed by the grid: `bold`, `italic` and
`underline` are booleans; `fontFamily`, `fill`, `color`, `alignment`,
`numberFormat` and `note` are strings. `fontSize` must be a finite number from
1 to 400. Numeric fractional sizes remain valid because OOXML imports use
numeric sizes, including floating-point values. Unknown style fields remain
preserved. Constructor, transaction and preview validation use the same guard.

## TDD evidence

The original regression uses a valid chart range and `anchor.row:"bad"` in a
batch that first writes a cell. Before implementation, the focused command
failed with `malformed chart anchor must be rejected before commit`. After
canonical validation was implemented, the same test passed and verified that
both transaction and preview rejection are atomic.

Additional boundary coverage verifies array/object and string field types,
missing/reversed/out-of-bounds ranges, chart-data limits, pixel bounds, integer
overflow, constructor validation, and acceptance at the actual chart-data
maximum. The existing session suite, including minimal legacy chart records,
continued to pass.

The subsequent VBA regression first failed with `malformed VBA metadata must
be rejected before commit`. It tests malformed containers, flags and module
fields, with a preceding cell edit to verify atomic rejection, and also checks
preview rejection and valid opaque-project metadata.

The cell-style regression first failed with `malformed cell style must be
rejected before commit`. Its matrix covers malformed consumed fields,
out-of-range and non-finite sizes, constructor rejection, atomic transaction
rejection, and preview rejection. The valid case retains an 11.5-point size
and an opaque extension object unchanged. The same focused command then
passed after the style guard was added.

Final observed GREEN: `session tests passed`. The complete focused build with
`-Wall -Wextra -Wpedantic` produced no warnings. No UI/CMake files changed.

```sh
g++ -std=c++20 -Iinclude tests/session_tests.cpp src/session.cpp \
  src/references.cpp -o /tmp/tigersheets-session-metadata-tests
/tmp/tigersheets-session-metadata-tests
```
