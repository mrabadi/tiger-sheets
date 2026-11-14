// TigerSheets build-only patch: the vendored source has no upstream .git tree.
// Embed the audited release, rather than querying the containing application's
// repository or repeatedly rebuilding because .git/HEAD is absent.
fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rustc-env=GIT_VERSION=v0.9.1");
}
