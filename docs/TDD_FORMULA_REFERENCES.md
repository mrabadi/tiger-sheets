# Formula reference spans and coherent F4 ranges

`formulaReferences(std::string_view)` returns immutable source descriptions: `start` and `length` are UTF-8 byte offsets covering the whole qualifier/range; `row1,col1,row2,col2` describe a normalized, inclusive, 1-based rectangle; `sheet` is the decoded worksheet name (empty for local references); `external` marks a workbook qualifier, which is removed from the decoded worksheet name. Callers using QString must convert UTF-16 cursor positions to UTF-8 byte offsets.

The scanner handles relative, absolute and mixed A1 cells/ranges, quoted sheet names with doubled apostrophes and Unicode, unquoted sheet names, external workbook/path qualifiers, and whole-column/whole-row ranges without expanding cells. Whole-axis rectangles use the worksheet's maximum row/column bounds. Double-quoted literals (including doubled quotes), structured brackets, function names followed by `(`, non-reference identifiers, invalid worksheet coordinates, malformed partial ranges and unsupported 3D sheet qualifiers do not produce partial references. Empty/incomplete formula text is bounded. The scanner reports lexical references; it does not resolve names, structured data, dynamic expressions or 3D references, and is not a complete Excel AST.

F4 uses the same parsed A1 cell spans. When the byte caret falls within the cell/range portion, both endpoints cycle together from the first endpoint's current lock mode: `A1:B3` → `$A$1:$B$3` → `A$1:B$3` → `$A1:$B3` → `A1:B3`. Mixed endpoint locks normalize to that shared next mode. The qualifier, letter case and separator whitespace remain unchanged, as do other ranges. Carets within sheet qualifier text do not rewrite it. Single-cell cycling, including external single cells, remains supported. Whole-axis F4 behavior remains unchanged. Existing shift and rename implementations were not modified.

## Executed TDD cycles

1. Local parser RED: the deliberately new public `formulaReferences` symbol was absent. Adding byte spans, A1 coordinates and lexical exclusions made the focused test pass.
2. Qualifier RED: sheet/external references were missing. Qualified spans and decoded sheet/workbook handling then passed.
3. Whole-axis RED: full-row/full-column ranges were absent. Bounded coordinate rectangles then passed.
4. Boundary RED: invalid ranges/3D qualifiers created false outlines. Token boundaries and whole-range validation then passed, including reversed endpoints and worksheet limits.
5. F4 RED: only one range endpoint changed. Shared parsing and coherent replacement of both endpoints then passed all four lock modes, qualified/mixed ranges, source formatting and caret isolation.

Commands were isolated direct C++20 compilations of `tests/references_tests.cpp` and `src/references.cpp` with `-Wall -Wextra -Wpedantic -Werror`. The complete focused binary passed normally and again with AddressSanitizer/UndefinedBehaviorSanitizer (`-fsanitize=address,undefined -fno-omit-frame-pointer`, leak detection disabled for the existing environment limitation). No Qt dependency, MainWindow, grid, or CMake edit was added. Durable raw evidence: `docs/validation/formula-references-tdd.log`. Host outline/completion behavior is verified separately.
