# Native Solver implementation

TigerSheets Solver proposes changing-cell values from an isolated workbook snapshot. Every objective and constraint evaluation uses the application's actual `CalculationEngine`; the source workbook is never modified. Applying a result must use the host's revision-checked preview/accept workflow.

## Public contract

`solveWorkbook(snapshot, specification)` returns `status`, `objective`, `feasibility`, `changingCells` and `evaluations`; validation/runtime failures also include `message`. Changing-cell entries contain `sheet`, 1-based `row`/`col` and a numeric `input` string suitable for a native `set_cell` operation.

The specification includes:

- `objective`: `{sheet, row, col}`.
- `variables`: 1–200 unique numeric-constant cells, each with `sheet`, `row`, `col`, optional `lower`, `upper`, `integer` and `binary` flags.
- `constraints`: up to 1000 `{sheet, row, col, relation, value}` records; relation is `<=`, `>=` or `=`.
- `goal`: `min`, `max` or `value`; `value` requires a finite numeric `target`.
- `method`: `linear`, `nonlinear` or `evolutionary`.
- `seed` (default 42), `maxEvaluations` (default 10000, maximum 1000000), and `tolerance` (default 1e-7).

Nonlinear and evolutionary methods require explicit finite variable bounds. Binary bounds are intersected with [0, 1]; integer bounds are rounded inward. Invalid addresses, repeated variables, reversed/empty bounds, nonnumeric changing cells and formula errors are explicit failures. A returned candidate is recalculated and checked against the original constraints, bounds, integrality and target before `feasibility` is true.

## Algorithms and dependencies

The pinned dependency ledger is `third_party/SOLVER_DEPS.json`; `cmake/SolverDependencies.cmake` resolves their exact local versions and exports `tigersheets_solver_dependencies` plus the shared runtime file list. Dependencies are build-time local libraries, with no network at runtime and no external office-suite backend.

HiGHS 1.15.1 handles linear and mixed-integer linear models. Affine coefficients come from workbook evaluations at the starting point and independent variable probes. Midpoint, combined local and bounded-domain probes reject common nonlinear/interacting expressions at the configured tolerance; this is a consistency check on a user-declared linear model, not a mathematical proof that arbitrary formulas are globally affine. HiGHS results are independently recalculated against the real workbook.

NLopt 2.11.0 is built with `NLOPT_LUKSAN=OFF`, selecting the MIT-licensed build. The continuous nonlinear method uses derivative-free COBYLA. Integer nonlinear models belong to the evolutionary method.

pagmo 2.20.0's Improved Harmony Search handles bounded nonlinear problems with mixed continuous/integer coordinates, equality and inequality constraints. It uses a controlled seed and a shared library build under LGPL-3.0-or-later. Its shared dependencies are oneTBB 2021.5.0 (Apache-2.0) and Boost.Serialization 1.74.0 (BSL-1.0), matching Ubuntu's Boost headers. Nonlinear/evolutionary success does not establish a global optimum.

Release packaging must ship the shared pagmo/TBB/Boost runtimes in its application library directory, set the runtime loader path appropriately, and preserve dependency licenses and required source/relinking availability. HiGHS and the configured NLopt library use MIT terms.

## Test-first evidence

The first verified RED was:

```sh
g++ -std=c++20 -Iinclude tests/solver_tests.cpp -o /tmp/tigersheets-solver-tests
```

The linker reported the deliberately missing public `tigersheets::solveWorkbook` symbol. The first acceptance case is a genuine LP: maximize `3*A1 + 2*B1` with `A1+B1 <= 4`, `0 <= A1 <= 2`, `0 <= B1 <= 10`; expected objective 10, two proposed changing cells, true feasibility, and an unchanged source snapshot.

### Observed RED → GREEN cycles

1. **Linear:** missing `solveWorkbook` public symbol produced the verified linker RED above. The first real library run exposed a genuine sparse-matrix construction error: appending an initial zero duplicated HiGHS' default start entry and discarded the constraint row. Assigning the start vector to `{0}` fixed it. The acceptance case then passed with objective 10, real feasibility and unchanged source state.
2. **Continuous nonlinear:** `/tmp/tigersheets-solver-tests nonlinear` failed with status `error`, message “Solver method is not implemented”, and zero evaluations. After wiring real NLopt COBYLA callbacks to workbook recalculation, minimizing `(A1-3)^2` with `A1 >= 1` and [-10, 10] bounds passed: objective below 1e-9, A1 within 1e-4 of 3, bounded evaluations and unchanged source state.
3. **Mixed-integer evolutionary:** `/tmp/tigersheets-solver-tests evolutionary` failed with the same missing-method result. Real pagmo IHS then passed a constrained mixed-integer nonlinear problem whose known objective minimum is 0.16. It found an objective below 0.18, kept the integer variable exactly 2, respected the original constraint, stayed within 3000 evaluations, reproduced the exact result with the same seed, and left the source unchanged. Integer coordinates are explicitly mapped to pagmo's final-coordinate convention and mapped back to source order in returned changing cells.
4. **Linearity regression:** `/tmp/tiger-native-solver-build/solver_tests linearity` failed “Global bounded probes must reject small curvature that local unit probes miss.” The model `0.00000001*A1^2` on [-10, 10] could otherwise be called linear from shallow probes and produce a false optimum. Added probes across declared bounds and tied consistency checks to the requested tolerance. The focused regression and complete solver test executable passed afterward.

Additional passing scenarios cover binary/integer LP variables, inward-rounded integer bounds, exact target-value goals, infeasible LPs with no proposed changes, nonlinear objective refusal in linear mode, formula-error refusal, bounds validation before calculation, continuous equality constraints, explicit rejection of integer models by the continuous backend, and evolutionary integer/equality constraints.

### Final verification

A standalone CMake verification project included the real `cmake/SolverDependencies.cmake`, used the current `src/solver.cpp` and `src/calculation.cpp`, and linked the current native IronCalc archive. Configure discovered the exact pinned optimization libraries; compilation with `-Wall -Wextra -Wpedantic` completed without warnings. Commands:

```sh
cmake -S /tmp/tiger-native-solver-cmake -B /tmp/tiger-native-solver-build -G Ninja \
  -DTIGERSHEETS_SOLVER_PREFIX="$PWD/.deps/solvers/prefix"
cmake --build /tmp/tiger-native-solver-build -j2
ctest --test-dir /tmp/tiger-native-solver-build --output-on-failure
```

Final result: **1/1 CTest passed**, exercising all scenarios in `tests/solver_tests.cpp`. The final focused linearity test also printed `solver tests passed`.

The fresh-source build initially encountered an unrelated concurrent engine ABI refresh (`tigersheets_calc_calculate_with_functions` missing from the old Rust archive). It passed after that archive was rebuilt. The first standalone shared-library run also caught a deployment issue: executable RUNPATH alone did not find pagmo's transitive Boost library. pagmo was reinstalled with `CMAKE_INSTALL_RPATH=$ORIGIN`; the final CMake test runs successfully without `LD_LIBRARY_PATH`.

## Limits

The continuous method reports convergence or a resource limit, and the evolutionary method reports a feasible candidate rather than claiming a proven global optimum. Only a declared affine model solved optimally by HiGHS reports `optimal`. Probe validation cannot prove arbitrary spreadsheet programs affine. Formula errors and exhausted budgets remain explicit failures. Solver currently evaluates built-in workbook formulas through `CalculationEngine`; host VBA/UDF callbacks, interactive cancellation and progress events are not exposed by this synchronous public function.
