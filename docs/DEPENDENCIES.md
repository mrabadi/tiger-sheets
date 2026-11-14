# Rebuilding the local solver dependencies

`tools/build_solver_dependencies.py` builds the exact archives pinned in `third_party/SOLVER_DEPS.json`. It uses Python's standard library, CMake, and the installed C/C++ toolchain. It never installs system packages or invokes `sudo`. By default all output stays in `.deps/solvers`, and the installed libraries go in `.deps/solvers/prefix`.

The validated baseline is Ubuntu 22.04.5, GCC 11.4, CMake 3.22.1, Python 3.10.12, and Boost headers 1.74.0. CMake 3.20 or newer and matching Boost 1.74 headers are required. The script verifies `boost/version.hpp` before compiling. Boost.Serialization's component source archive uses the remaining installed Boost headers; pass `--boost-include /local/boost/include` if the matching headers are installed elsewhere. No other office application is used.

## Repeatable commands

Run from the TigerSheets repository. With all five archives already present in `.deps/solvers/downloads`, the complete build is offline:

```sh
python3 tools/build_solver_dependencies.py --verify-only
python3 tools/build_solver_dependencies.py --jobs 2
cmake -S . -B build -DTIGERSHEETS_SOLVER_PREFIX="$PWD/.deps/solvers/prefix"
cmake --build build --parallel 2
```

On a fresh checkout, explicitly permit the first download:

```sh
python3 tools/build_solver_dependencies.py --download --jobs 2
```

`--download` fetches only missing archives from their pinned HTTPS URLs. Existing damaged archives cause an error; they are not silently replaced. Every archive must match its full SHA-256 before extraction or use. `--prepare-only` verifies, extracts, and preserves source/license material without compiling. `--verify-only` verifies archives without creating source or install directories.

To validate a new build without replacing a working installation:

```sh
python3 tools/build_solver_dependencies.py --jobs 2 \
  --prefix .deps/solvers/rebuild-prefix
cmake -S . -B build-rebuilt \
  -DTIGERSHEETS_SOLVER_PREFIX="$PWD/.deps/solvers/rebuild-prefix"
cmake --build build-rebuilt --parallel 2
```

Use a separate `--work-dir` as well when two dependency builds must run concurrently. Archive names are `highs-v1.15.1.tar.gz`, `nlopt-v2.11.0.tar.gz`, `pagmo-v2.20.0.tar.gz`, `onetbb-v2021.5.0.tar.gz`, and `boost-serialization-1.74.0.tar.gz`. Copy them into the selected work directory's `downloads` subdirectory for an offline build. Each run extracts fresh sources from those archives and uses stable build directories under `WORK_DIR/build`; CMake configuration is retained, while refreshed source timestamps may require recompilation.

The bootstrap uses argument arrays instead of a shell for all CMake commands. Archive extraction rejects absolute paths, parent traversal, duplicate paths, multiple upstream roots, symbolic/hard links, and special files before writing any source files. Unpacking is staged before its destination appears. Upstream executable file permissions are retained without preserving ownership or setuid bits.

## Exact library configuration

| Library | Build configuration |
| --- | --- |
| HiGHS 1.15.1 | Static `highs` and `highs_extras`; tests, CLI, Fortran, and zlib disabled; default solver threads disabled |
| NLopt 2.11.0 | Static; `NLOPT_LUKSAN=OFF`; tests and Python/Guile/MATLAB/Octave/SWIG/Java/Fortran bindings disabled |
| oneTBB 2021.5.0 | Shared; tests and separate TBB malloc library disabled |
| Boost.Serialization 1.74.0 | Shared; all component `src/*.cpp` files; matching Boost headers; `BOOST_SERIALIZATION_DYN_LINK`; SONAME 1.74.0 |
| pagmo 2.20.0 | Shared; NLopt/Eigen/IPOPT integrations, tests/tutorials/benchmarks, and IPO disabled; local Boost.Serialization/TBB; installed runtime path `$ORIGIN` |

All are Release builds with position-independent code and local `lib` installation paths. The script contains the complete configure commands and the small Boost.Serialization CMake wrapper. `third_party/SOLVER_DEPS.json` also records options that must not be changed silently.

The application links `highs::highs`, `NLopt::nlopt`, and `Pagmo::pagmo` through `cmake/SolverDependencies.cmake`. Preserve `libpagmo.so*`, `libtbb.so*`, and `libboost_serialization.so*` together in the release runtime directory. The shared pagmo library's `$ORIGIN` runtime path resolves its companions from the same directory.

## Sources, licenses, and verification

The installed prefix contains:

- `share/tiger-sheets/sources`: the exact original archives, `SOLVER_DEPS.json`, and the bootstrap script.
- `share/tiger-sheets/licenses/solvers`: upstream license and notice files plus the common Boost license.

Tracked license copies and provenance are under `third_party/licenses/solvers`. Include these source and license directories in release packages. An installed source bundle can be rebuilt by copying its five archives into a local work directory's `downloads` directory and invoking its preserved script with `--manifest` pointing to its preserved `SOLVER_DEPS.json`.

Run the offline bootstrap tests with:

```sh
python3 tests/dependency_bootstrap_tests.py
```

Ten tests cover checksum refusal, no implicit network access, verified atomic downloads, safe extraction, local build options and destinations, source/license preservation, CLI failure behavior, and an actual small CMake shared-library build/install. Six observed RED→GREEN cycles covered the missing implementation, extraction, network/build planning, executable CLI and source preservation, isolated prefix, and HiGHS' separate static-extras option. Binary byte-for-byte reproducibility is not claimed across toolchains or systems.

### Executed native rebuild

On the baseline above, `python3 tools/build_solver_dependencies.py --jobs 2 --prefix .deps/solvers/rebuild-prefix` completed offline with exit 0, rebuilding and installing all five libraries. It used fresh source extraction and separate build directories; the existing working prefix's binaries were left intact. The complete local log is `/tmp/tiger-solver-bootstrap.log`.

The actual repository `tests/solver_tests.cpp`, `src/solver.cpp`, and `src/calculation.cpp` were then compiled in a standalone CMake verification target against the rebuilt prefix and the pinned native calculation archive. The warning-enabled build completed, CTest passed 1/1 in 1.19 seconds, and direct execution with `LD_LIBRARY_PATH` removed printed `solver tests passed`. This exercises real HiGHS LP/MIP, NLopt COBYLA, and pagmo IHS callbacks rather than stub optimizer calls. `readelf -d` on the rebuilt `libpagmo.so.9.0` confirmed `RUNPATH [$ORIGIN]`.

Artifacts: `/tmp/tiger-native-solver-rebuild-build/solver_tests` and `/tmp/tiger-native-solver-rebuild-build/Testing/Temporary/LastTest.log`. For a regular repository build configured against the rebuilt prefix, the corresponding check is `ctest --test-dir build-rebuilt -R '^solver_tests$' --output-on-failure`.
