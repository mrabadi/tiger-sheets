# Calculation bridge validation

TigerSheets uses the standalone IronCalc base crate pinned at
`eb9ca75bd99160c17902359d2d87225c4571c468`, behind an owned Rust C ABI and C++
adapter. This is an actual local evaluator; no office application, server, or
cached spreadsheet result is used. Application extension patches are recorded in
`third_party/ironcalc/PINNED.md`.

## Toolchain and dependency preparation

The implementation environment initially had no Rust compiler. Official Rust
1.99.1 was installed locally under `/tmp/tigersheets-toolchain`, without changing
the system or shell configuration. Registry dependencies were fetched into that
local Cargo home. The bridge's `Cargo.lock` records their checksums.

Commands below use the implementation environment's local toolchain:

```sh
export PATH=/tmp/tigersheets-toolchain/cargo/bin:$PATH
export CARGO_HOME=/tmp/tigersheets-toolchain/cargo
export RUSTUP_HOME=/tmp/tigersheets-toolchain/rustup
export CARGO_BUILD_JOBS=2
cargo build --offline --locked --release --manifest-path rust/ironcalc_bridge/Cargo.toml
g++ -std=c++20 -Iinclude tests/calculation_tests.cpp src/calculation.cpp \
  rust/ironcalc_bridge/target/release/libtigersheets_calc_bridge.a \
  -ldl -lpthread -lm -o /tmp/tigersheets-calculation-tests
/tmp/tigersheets-calculation-tests
```

Future development machines may use their normal installed Cargo/Rust compiler;
the `/tmp` paths are not a runtime dependency. The static calculation library is
linked into the application.

## RED evidence

1. Cross-sheet calculation test: sheet `Inputs` holds `10` and `20`; another
   sheet computes `=SUM(Inputs!A1:A2)*2`. It must return numeric `60`, the stable
   sheet ID, and revision `37`.
2. Before the implementation, the focused command
   `g++ -std=c++20 -Iinclude tests/calculation_tests.cpp -o /tmp/tigersheets-calculation-tests`
   failed with an undefined reference to
   `tigersheets::CalculationEngine::calculate(...) const`. The new public
   behavior was missing; compilation of the test itself succeeded.

## Verified behavior cycles

All focused cycles use the compiled C++ adapter and real Rust static library.

| Behavior | Observed RED | Observed GREEN |
| --- | --- | --- |
| Cross-sheet SUM and revision | Undefined new calculation symbol | Numeric 60, revision 37 |
| Formula errors remain typed | `A formula error must not become plain text` | `#DIV/0!` typed error and diagnostic |
| Pinned INFO release | Wrong parent repository release | `v0.9.1` |
| Dynamic array spill output | `Expected calculated cell missing` | SEQUENCE spill and SUM(A1#) returned |
| Atomic snapshot validation | Invalid snapshot accepted | Empty/duplicate/out-of-bounds rejected with no values |
| Per-instance cancellation/reset | Missing new cancel/reset methods | Cancelled engine publishes no values; independent engine works; reset restores calculation |
| Auditable function catalog | Missing new functions method | 497 sorted names with exact commit; availability-only flag |
| Structurally invalid input keeps revision | `Rejected snapshots must retain their revision` | Numeric input rejected, revision 42 retained |
| Scalar custom function AST calls | Missing new calculate(snapshot, evaluator) overload | Nested built-in/custom arithmetic 78 and dependent 84, raw formulas preserved |
| Circular iteration | `Enabled iteration must evaluate circular formulas numerically` | Coupled cycle including INDIRECT converges to 10/10 and dependent 20; disabled mode returns #CIRC! |
| Local workbook links | `Linked workbooks must evaluate their source formulas and cross-sheet dependencies` | Loaded source formulas calculate through transient aliases; string literals and canonical formulas preserved |
| Global workbook names | `Workbook named ranges must resolve inside financial formulas` | Named range IRR and named LAMBDA composition resolve with either leading '=' convention |

The final formula extension cycle reproduced both independent reference-runner
failures before production changes: `AGGREGATE(9,0,A1:A5)` returned `#NAME?`
instead of 15, and `TEXTJOIN(",",TRUE,{"a","b"})` returned `#N/IMPL!`
instead of `a,b`. Command:

```sh
build/formula_catalog_tests tests/fixtures/formulas.json \
  /tmp/tigersheets-formula-red.json
```

Observed RED: **280/282 reference cases passed**, with exactly those two failed
cases. The owned extension additionally checks all 19 AGGREGATE statistics,
ignored errors/nested subtotals and hidden rows. The available-function catalog
includes 497 pinned upstream names plus the owned `AGGREGATE` extension.

Fresh-linking the same reference runner against the extended engine produced
**282/282 reference cases passed**, report
`/tmp/tigersheets-formula-green.json`. The broader 19-mode acceptance fixture
then detected a real additional failure: AGGREGATE mode 6 inherited the
upstream PRODUCT array `#N/IMPL!`. Its adapter was corrected to send collected
numeric operands through the engine's scalar PRODUCT operation.
The final release archive passed the complete focused calculation suite,
including all 19 AGGREGATE modes and its hidden/error/nested-subtotal examples.

The finance acceptance fixture verifies actual IRR, XIRR, XNPV, NPV, PMT,
XLOOKUP, INDEX/MATCH and LET against a two-date cash flow. A formula registry
entry is not a claim that every Excel edge case is conformant.

## Upstream verification

The pinned base crate, with only its deterministic build metadata patch, was
copied to `/tmp/tigersheets-ironcalc-conformance-v0.9.1` and tested offline with
the same local toolchain and `CARGO_BUILD_JOBS=2`:

```sh
cargo test --offline --locked \
  --manifest-path /tmp/tigersheets-ironcalc-conformance-v0.9.1/Cargo.toml
```

Observed result: **2,765 unit tests passed, 8 ignored; 23 doc tests passed;
0 failed**. Full log: `/tmp/tigersheets-ironcalc-conformance.log`.
This establishes the pinned upstream baseline; subsequent evaluator extension
patches require their own focused and regression verification.

The complete upstream suite was rerun after adding both the AST callback and
opt-in circular iteration extensions. Observed result remained **2,765 unit
tests passed, 8 ignored; 23 doc tests passed; 0 failed**. Full regression log:
`/tmp/tigersheets-ironcalc-extension-regression.log`.

The complete suite was run again against the final source including AGGREGATE,
TEXTJOIN arrays and the PRODUCT adapter repair. Observed final result:
**2,765 unit tests passed, 8 ignored; 23 doc tests passed; 0 failed**.
Log: `/tmp/tigersheets-ironcalc-final-regression.log`.

## Host contracts

`CalculationEngine::calculate(snapshot, evaluator)` calls a synchronous C++
function with the unknown worksheet function name and JSON scalar arguments.
It returns `{status:"completed",value:<scalar>}` or an error response. Arguments
and nested/dependent formulas evaluate inside IronCalc. Host exceptions become
typed `#VALUE!` formula errors. A plain `calculate(snapshot)` installs no host
function callback. The host controls macro enablement and supplies its read-only
VBA evaluator; this layer never authorizes macros itself.

Circular iteration reads `settings.iterationEnabled`, `maxIterations` (default
100, range 1..100000), and `maxChange` (default 0.001, nonnegative). Recursive
reads use a previous-pass value, initially zero. Results include
`iteration:{iterations,converged,maxChange}`. Limit exhaustion publishes the
actual last pass and an `iteration_not_converged` diagnostic. The divergent
`=A1+1` characterization fixture verifies three passes return 3 without claiming
convergence. Error results never count as convergence.

Local links are transient caller-loaded data:

```json
{"links":[{"name":"Inputs.xlsx","path":"/local/Inputs.xlsx","enabled":true,
  "workbook":{"revision":1,"sheets":[{"id":"inputs","name":"Inputs","cells":[]}]}}]}
```

The calculation layer does not open files or fetch network data. It aliases the
provided source sheets on a calculation copy, including their own cross-sheet
references. It returns only root-workbook cells. Disabled links use only an
explicit `cachedWorkbook` containing literal cell inputs and emit
`external_link_cached`. Cached formulas are rejected. Missing sources return
`#REF!` with `external_link_unavailable`. Characterization fixtures cover cached
vs live source selection, missing links, and host exceptions.

## Current limitations

Cancellation is checked before and after the synchronous engine evaluation and
while importing/exporting cells. It does not interrupt the interior of an
individual upstream evaluation. Reset or destruction must wait until the active
calculation finishes. C ABI panics are caught; malformed snapshots return
explicit diagnostics instead of partial values.

The callback currently accepts scalar arguments/results; ranges and arrays need
an extended host contract. Circular dynamic-array spill topology still follows
upstream circular-reference behavior and does not claim Excel iterative-array
conformance. Iteration uses zero seeds and an absolute scalar tolerance; Excel
cached starting values and all historical calculation-order edge cases are not
validated. OOXML external references such as `[1]Sheet!A1` need relationship
resolution to workbook filenames by the I/O host before this layer can resolve
them. Workbook link names must be unambiguous across loaded sources.

Global `names:{name:formulaString}` imports range references and LAMBDA
definitions through the engine's public defined-name API. The underlying engine
rejects constant/arithmetic defined-name formulas; these remain unsupported.
The engine's date conversions use the 1900 epoch; `settings.date1904` is retained
by workbook I/O but is not implemented by this evaluator. The C++ adapter now
refuses snapshots declaring `date1904:true` with status
`unsupported_date_system`, the original revision, no calculated values and a
structured diagnostic. Both calculation overloads apply the guard before engine
or host-function evaluation. This is a safety refusal, not 1904 calculation
support; the release requirement remains incomplete. Thai BAHTTEXT,
DBCS/JIS, GETPIVOTDATA and PHONETIC are absent from the available registry.
The presence of a function and a passing reference case does not establish
complete function conformance. AGGREGATE filtered-row distinctions and all
computed-array/nesting edge cases require further validation.

## 1904 date-system safety regression

The actual-engine fixture sets `date1904:true` and includes `YEAR(A1)` with
serial zero and `DATE(1904,1,1)`. Before the guard, the focused suite failed with
`1904 workbooks must not silently calculate using the 1900 date epoch` because
the engine returned normal calculation status. After adding the adapter guard,
the full focused suite passed. It verifies revision retention, empty values,
structured diagnostics, identical behavior with a custom-function evaluator,
and no host callback invocation. Setting `date1904:false` still calculates
`DATE(1904,1,1)` as 1462 under the existing 1900 epoch.

The final `-Wall -Wextra -Wpedantic` build and run returned
`calculation tests passed` without warnings. No Rust source or function catalog
changed for this guard.
