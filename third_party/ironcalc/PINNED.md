# IronCalc calculation dependency

Upstream: https://github.com/ironcalc/IronCalc
Release: v0.9.1 (October 21, 2026)
Commit: eb9ca75bd99160c17902359d2d87225c4571c468
Archive: https://codeload.github.com/ironcalc/IronCalc/tar.gz/eb9ca75bd99160c17902359d2d87225c4571c468
Archive SHA-256: 279fba8eb6eaaabab3beae2151e0931ff17d1b1cac0bd7b426dea4ac110de8c5

Only the `base/` crate, upstream lockfile, and upstream licenses are vendored.
The build metadata patch replaces `base/build.rs`'s parent-repository Git
lookup with the fixed audited release `v0.9.1`. This keeps INFO("release") correct
and prevents Cargo rebuilding the whole engine on every call because upstream
`.git` files are absent.

An application extension in `base/src/model.rs` and `base/src/new_empty.rs`
adds an optional synchronous callback for unknown scalar function AST nodes.
Built-in functions, LET bindings and defined-name lambdas keep their existing
resolution priority. Arguments are evaluated by the engine, errors propagate,
and non-scalar arguments return an explicit error. The callback is absent in
ordinary upstream evaluation. The application owns the C ABI and host callback.

`base/src/functions/aggregate.rs`, its module registration and the unknown-name
AST dispatch add the TigerSheets-owned AGGREGATE extension. It dispatches all
19 statistics to the actual existing engine operations after filtering supplied
reference data according to options. Its function name is reported separately
in the bridge catalog's `extensions` as well as in available `functions`.
`base/src/functions/text/common.rs` adds scalar flattening of TEXTJOIN array
arguments, preserving row-major order, blank handling and error propagation.
These repair independently reproduced formula conformance failures.

`base/src/evaluation.rs` adds opt-in `evaluate_iterative`. Recursive reads that
close a loop use prior-pass values initialized to zero; each pass otherwise uses
the ordinary evaluator. Passes stop when every formula's scalar result changes
by at most the configured absolute tolerance, or at the iteration limit. Error
values do not count as convergence. Ordinary `evaluate()` retains circular
errors and the upstream behavior when this opt-in method is not called.
The application uses no upstream UI or office application. See
`base/THIRD-PARTY-NOTICES.md` for Unicode CLDR data attribution.
