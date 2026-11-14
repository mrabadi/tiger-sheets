# Offline Rust dependencies

The calculation bridge's locked crates.io dependencies are checked in under
`third_party/rust-vendor`. The root `.cargo/config.toml` replaces crates.io
with that directory. Run Cargo from the repository root or a subdirectory so
Cargo discovers this configuration. No Cargo registry cache or network access
is needed to resolve this lockfile.

`rust/ironcalc_bridge/Cargo.lock` remains the version and package-checksum
authority. The vendor directory contains all 72 registry packages in that
lockfile, including target-specific dependencies. The path dependency
`third_party/ironcalc/base` remains separately pinned and contains the owned
calculation patches; it is not copied into the registry vendor directory.

Cargo's generated `.cargo-checksum.json` files are retained. An independent
SHA-256 verification checked all 72 package checksums against the lockfile and
all 3,327 listed source-file checksums. The copied source files total
52,491,424 bytes; 139 license, notice and copying files are retained. Individual
crate manifests and accompanying license files describe their upstream terms.
Do not remove these notices when distributing the source tree.

The vendor-root `.gitignore` preserves upstream source directories named
`target`. Without this exception, the repository's build-artifact ignore rule
excluded four `cc-1.9.0/src/target/*.rs` source files. The delivery audit first
detected those four ignored files, then passed with zero ignored vendored
files after the exception was added. No crate source/checksum files changed.

The vendor directory contains source dependencies, not a Rust toolchain,
system compiler, C++/Qt dependencies or build artifacts. Rust and Cargo
1.99.1 were used for the verification below. No toolchain was installed
globally during this task, and no networking was used.

## Verified dependency regression

The required behavior is a fresh locked offline check without a populated
Cargo registry. Before adding the vendor directory and source replacement,
this command used an empty isolated Cargo home and a separate target directory:

```sh
env CARGO_HOME=/tmp/tigersheets-vendor-empty-cargo \
  RUSTUP_HOME=/tmp/tigersheets-toolchain/rustup \
  PATH=/tmp/tigersheets-toolchain/cargo/bin:/usr/bin:/bin \
  CARGO_TARGET_DIR=/tmp/tigersheets-vendor-offline-target \
  CARGO_BUILD_JOBS=2 \
  cargo check --offline --locked \
    --manifest-path rust/ironcalc_bridge/Cargo.toml
```

Observed RED: exit 101, `no matching package named serde found`, with
`location searched: crates.io index`. This reproduces the missing offline
dependency-delivery behavior rather than a calculation test failure.

The sources were then generated from the existing populated local cache:

```sh
env CARGO_HOME=/tmp/tigersheets-toolchain/cargo \
  RUSTUP_HOME=/tmp/tigersheets-toolchain/rustup \
  PATH=/tmp/tigersheets-toolchain/cargo/bin:/usr/bin:/bin \
  CARGO_BUILD_JOBS=2 \
  cargo vendor --offline --locked --versioned-dirs \
    --manifest-path rust/ironcalc_bridge/Cargo.toml \
    third_party/rust-vendor
```

The source replacement follows Cargo's emitted configuration exactly. The
same isolated-cache check command is the GREEN acceptance check; its target
directory is separate from the existing bridge build so cached compiler
outputs cannot satisfy the check.

Observed GREEN: exit 0, `Finished dev profile [unoptimized + debuginfo]` in
29.51 seconds after a fresh compilation. The isolated Cargo registry directory
contained only `CACHEDIR.TAG`; it contained no registry index, crate archives
or unpacked source cache. The check produced no compiler warnings. The same
command passed again in 0.10 seconds after the ignore-rule delivery fix.

## Rechecking on another machine

With a supported installed Rust toolchain on `PATH`, choose empty task-specific
`CARGO_HOME` and `CARGO_TARGET_DIR` directories, then run the same
`cargo check --offline --locked --manifest-path rust/ironcalc_bridge/Cargo.toml`
from the repository root. If using rustup, retain the existing `RUSTUP_HOME`
that contains the toolchain. The temporary paths above record this session's
environment; they are not required source locations.

When deliberately updating dependencies, update and review the lockfile and
vendor directory together, preserve all upstream license/checksum files, and
repeat the isolated-cache offline check. This task changes dependency
delivery only; it does not establish further spreadsheet compatibility.
