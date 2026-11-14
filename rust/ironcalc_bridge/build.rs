use std::{env, fs, path::PathBuf};

fn main() {
    let registry = "../../third_party/ironcalc/base/src/functions/mod.rs";
    println!("cargo:rerun-if-changed={registry}");
    println!("cargo:rerun-if-changed=build.rs");
    let source = fs::read_to_string(registry).expect("Pinned IronCalc registry must be present");
    let registrations = source
        .split_once("impl_function_lookup! {")
        .expect("Pinned IronCalc function lookup declaration")
        .1
        .split_once("\n}")
        .expect("Function lookup declaration terminator")
        .0;
    let fields: Vec<_> = registrations
        .lines()
        .filter_map(|line| line.split_once("=>").map(|(field, _)| field.trim()))
        .collect();
    let entries = fields
        .iter()
        .map(|field| format!("functions.{field}.clone()"))
        .collect::<Vec<_>>()
        .join(",\n");
    let generated = format!(
        "fn registered_functions() -> Vec<String> {{\n\
         let functions = &ironcalc_base::language::get_default_language().functions;\n\
         let mut names = vec![{entries}];\n\
         names.sort(); names.dedup(); names\n}}\n"
    );
    fs::write(
        PathBuf::from(env::var_os("OUT_DIR").expect("Cargo OUT_DIR"))
            .join("registered_functions.rs"),
        generated,
    )
    .expect("Write generated registered function catalog");
}
