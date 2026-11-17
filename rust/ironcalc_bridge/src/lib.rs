use ironcalc_base::{cell::CellValue, types::CellType, Model};
use serde::Deserialize;
use serde_json::{json, Value};
use std::collections::{BTreeMap, HashMap, HashSet};
use std::ffi::{c_char, c_void, CStr, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

include!(concat!(env!("OUT_DIR"), "/registered_functions.rs"));

#[derive(Clone, Deserialize)]
struct Snapshot {
    revision: Value,
    sheets: Vec<Sheet>,
    #[serde(default)]
    settings: Value,
    #[serde(default)]
    links: Vec<WorkbookLink>,
    #[serde(default)]
    names: HashMap<String, String>,
    #[serde(default, rename = "_affineQuery")]
    affine_query: Option<Value>,
    #[serde(default, rename = "_tigersheetsComputedResults")]
    computed_results: bool,
}

#[derive(Clone, Deserialize)]
struct WorkbookLink {
    #[serde(default)]
    name: String,
    #[serde(default)]
    path: String,
    enabled: Option<bool>,
    workbook: Option<Box<Snapshot>>,
    #[serde(rename = "cachedWorkbook")]
    cached_workbook: Option<Box<Snapshot>>,
}

#[derive(Clone, Deserialize)]
struct Sheet {
    id: String,
    name: String,
    cells: Vec<InputCell>,
    #[serde(default, rename = "hiddenRows")]
    hidden_rows: Vec<i32>,
    #[serde(default, rename = "filteredRows")]
    filtered_rows: Vec<i32>,
}

#[derive(Clone, Deserialize)]
struct InputCell {
    row: i32,
    col: i32,
    input: String,
}

pub struct CalcHandle {
    cancelled: AtomicBool,
}

type FunctionCallback = unsafe extern "C" fn(*mut c_void, *const c_char) -> *const c_char;

fn linked_sheets(snapshot: &Snapshot) -> Result<(Vec<Sheet>, Vec<Value>), String> {
    fn collect(snapshot: &Snapshot, groups: &mut Vec<(String, Vec<Sheet>)>,
        visited: &mut HashMap<String, String>, diagnostics: &mut Vec<Value>, depth: usize) -> Result<(), String> {
        if depth > 32 { return Err("Workbook links exceed the nesting limit".into()); }
        for link in &snapshot.links {
            let name = if link.name.is_empty() { link.path.rsplit('/').next().unwrap_or("") } else { &link.name };
            if name.is_empty() { return Err("Linked workbooks need a name or local path".into()); }
            let key = name.to_lowercase();
            if let Some(path) = visited.get(&key) {
                if path != &link.path { return Err(format!("Ambiguous linked workbook name: {name}")); }
                continue;
            }
            visited.insert(key, link.path.clone());
            let enabled = link.enabled.unwrap_or(true);
            let source = if enabled { link.workbook.as_deref() } else { link.cached_workbook.as_deref() };
            if let Some(source) = source {
                if source.sheets.is_empty() { return Err(format!("Linked workbook has no sheets: {name}")); }
                if !enabled {
                    if source.sheets.iter().flat_map(|sheet| &sheet.cells).any(|cell| cell.input.starts_with('=')) {
                        return Err("Disabled workbook link caches must contain literal values".into());
                    }
                    diagnostics.push(json!({"code":"external_link_cached","workbook":name,
                        "message":"Disabled workbook link uses explicit cached values"}));
                }
                groups.push((name.to_string(), source.sheets.clone()));
                if enabled { collect(source, groups, visited, diagnostics, depth + 1)?; }
            } else {
                diagnostics.push(json!({"code":"external_link_unavailable","workbook":name,
                    "message":"Linked workbook source is unavailable"}));
            }
        }
        Ok(())
    }
    let mut groups = vec![(String::new(), snapshot.sheets.clone())];
    let mut diagnostics = Vec::new();
    collect(snapshot, &mut groups, &mut HashMap::new(), &mut diagnostics, 0)?;
    let mut claimed: HashSet<String> = snapshot.sheets.iter().map(|sheet| sheet.name.to_lowercase()).collect();
    let mut local_maps = Vec::new();
    let mut external_map = HashMap::new();
    for (group_index, (book, sheets)) in groups.iter_mut().enumerate() {
        let mut local = HashMap::new();
        for (sheet_index, sheet) in sheets.iter_mut().enumerate() {
            let original = sheet.name.clone();
            if group_index > 0 {
                let mut suffix = 0;
                loop {
                    let alias = format!("_TigerLink{group_index}_{sheet_index}_{suffix}");
                    if claimed.insert(alias.to_lowercase()) { sheet.name = alias; break; }
                    suffix += 1;
                }
                sheet.id = format!("_linked_{group_index}_{sheet_index}");
                external_map.insert(format!("[{book}]{original}").to_lowercase(), sheet.name.clone());
            }
            local.insert(original.to_lowercase(), sheet.name.clone());
        }
        local_maps.push(local);
    }
    let mut all_sheets = Vec::new();
    for (index, (_, mut sheets)) in groups.into_iter().enumerate() {
        for sheet in &mut sheets {
            for cell in &mut sheet.cells {
                if cell.input.starts_with('=') {
                    cell.input = remap_references(&cell.input, &local_maps[index], &external_map);
                }
            }
        }
        all_sheets.extend(sheets);
    }
    Ok((all_sheets, diagnostics))
}

fn remap_references(input: &str, local: &HashMap<String, String>, external: &HashMap<String, String>) -> String {
    let chars: Vec<char> = input.chars().collect();
    let mut result = String::new();
    let mut i = 0;
    while i < chars.len() {
        let start = i;
        if chars[i] == '"' {
            i += 1;
            while i < chars.len() {
                if chars[i] == '"' {
                    i += 1;
                    if i < chars.len() && chars[i] == '"' { i += 1; continue; }
                    break;
                }
                i += 1;
            }
            result.extend(chars[start..i].iter());
            continue;
        }
        let mut name = String::new();
        if chars[i] == '\'' {
            i += 1;
            while i < chars.len() {
                if chars[i] == '\'' {
                    i += 1;
                    if i < chars.len() && chars[i] == '\'' { name.push('\''); i += 1; continue; }
                    break;
                }
                name.push(chars[i]); i += 1;
            }
        } else if chars[i].is_alphanumeric() || "_[".contains(chars[i]) {
            while i < chars.len() && (chars[i].is_alphanumeric() || "_.[]$".contains(chars[i])) {
                name.push(chars[i]); i += 1;
            }
        } else {
            result.push(chars[i]); i += 1; continue;
        }
        if i < chars.len() && chars[i] == '!' {
            let normalized = name.rfind('[').map(|index| &name[index..]).unwrap_or(&name).to_lowercase();
            let alias = if normalized.starts_with('[') { external.get(&normalized) } else { local.get(&normalized) };
            if let Some(alias) = alias {
                result.push('\''); result.push_str(&alias.replace('\'', "''")); result.push_str("'!");
                i += 1;
                continue;
            }
        }
        result.extend(chars[start..i].iter());
    }
    result
}

fn reconstructible_literal(input: &str, value: &CellValue) -> bool {
    if input.starts_with('=') { return false; }
    if input.is_empty() { return matches!(value, CellValue::None); }
    if let Some(text) = input.strip_prefix('\'') {
        return matches!(value, CellValue::String(actual) if actual == text);
    }
    if input == "TRUE" || input == "FALSE" {
        return matches!(value, CellValue::Boolean(actual) if *actual == (input == "TRUE"));
    }
    // The C++ raw fallback uses from_chars, which does not accept a leading '+'.
    if !input.starts_with('+') {
        if let Ok(number) = input.parse::<f64>() {
            if number.is_finite() {
                return matches!(value, CellValue::Number(actual) if *actual == number);
            }
        }
    }
    matches!(value, CellValue::String(actual) if actual == input)
}

fn evaluate_snapshot(snapshot: Snapshot, cancelled: &AtomicBool,
    callback: Option<FunctionCallback>, reference_callback: Option<FunctionCallback>, context: *mut c_void) -> Result<Value, String> {
    let cancellation = || {
        cancelled.load(Ordering::Acquire).then(|| {
            error_result(snapshot.revision.clone(), "cancelled", "Calculation cancelled")
        })
    };
    if let Some(result) = cancellation() { return Ok(result); }
    if snapshot.sheets.is_empty() {
        return Err("A workbook must contain at least one sheet".into());
    }
    let root_sheet_count = snapshot.sheets.len();
    let (sheets, link_diagnostics) = linked_sheets(&snapshot)?;
    let mut identifiers = HashSet::new();
    for sheet in &sheets {
        if sheet.id.is_empty() || !identifiers.insert(&sheet.id) {
            return Err("Sheet IDs must be nonempty and unique".into());
        }
        let mut coordinates = HashSet::new();
        for cell in &sheet.cells {
            if let Some(result) = cancellation() { return Ok(result); }
            if !(1..=1_048_576).contains(&cell.row) || !(1..=16_384).contains(&cell.col) {
                return Err("Cell coordinates exceed the Excel worksheet bounds".into());
            }
            if !coordinates.insert((cell.row, cell.col)) {
                return Err("Cell coordinates must be unique within each sheet".into());
            }
        }
    }
    let mut model = Model::new_empty("TigerSheets", "en", "UTC", "en")?;
    if let Some(callback)=reference_callback {
        let identifiers:Vec<String>=sheets.iter().map(|sheet|sheet.id.clone()).collect();
        model.external_reference_function=Some(Arc::new(move |measure,reference,args| {
            let mut fields=Vec::new();
            for pair in args.chunks_exact(2) {
                let CellValue::String(field)=&pair[0] else {return Err("#REF!".into());};
                let item=match &pair[1] {
                    CellValue::Number(value)=>json!({"type":"number","value":value}),
                    CellValue::String(value)=>json!({"type":"string","value":value}),
                    CellValue::Boolean(value)=>json!({"type":"boolean","value":value}),
                    CellValue::None=>json!({"type":"blank","value":Value::Null}),
                };
                fields.push(json!({"field":field,"item":item}));
            }
            let sheet=identifiers.get(reference.sheet as usize).ok_or("#REF!")?;
            let request=CString::new(json!({"name":"__TIGERSHEETS_GETPIVOTDATA","args":{
                "sheet":sheet,"row":reference.row,"col":reference.column,"measure":measure,"fieldItems":fields}}).to_string()).map_err(|_|"#REF!")?;
            let response=unsafe {callback(context,request.as_ptr())};
            if response.is_null() {return Err("#REF!".into());}
            let result:Value=serde_json::from_str(unsafe {CStr::from_ptr(response)}.to_str().map_err(|_|"#REF!")?).map_err(|_|"#REF!")?;
            if result.get("status").and_then(Value::as_str)!=Some("completed") {return Err(result.get("code").and_then(Value::as_str).unwrap_or("#REF!").into());}
            match result.get("value").unwrap_or(&Value::Null) {
                Value::Number(value)=>value.as_f64().map(CellValue::Number).ok_or("#REF!".into()),
                Value::String(value)=>Ok(CellValue::String(value.clone())),
                Value::Bool(value)=>Ok(CellValue::Boolean(*value)),Value::Null=>Ok(CellValue::None),_=>Err("#REF!".into()),
            }
        }));
    }
    if let Some(callback) = callback {
        model.external_function = Some(Arc::new(move |name, args| {
            let args: Vec<Value> = args.into_iter().map(|arg| match arg {
                CellValue::Number(v) => json!(v), CellValue::String(v) => json!(v),
                CellValue::Boolean(v) => json!(v), CellValue::None => Value::Null,
            }).collect();
            let request = CString::new(json!({"name":name,"args":args}).to_string())
                .map_err(|_| "Invalid function request".to_string())?;
            // The C ABI promises live UTF-8 response storage for this synchronous call.
            let response = unsafe { callback(context, request.as_ptr()) };
            if response.is_null() { return Err("Function returned no response".into()); }
            let text = unsafe { CStr::from_ptr(response) }.to_str().map_err(|e| e.to_string())?;
            let result: Value = serde_json::from_str(text).map_err(|e| e.to_string())?;
            if result.get("status").and_then(Value::as_str) != Some("completed") {
                return Err(result.get("diagnostics").unwrap_or(&Value::Null).to_string());
            }
            match result.get("value").unwrap_or(&Value::Null) {
                Value::Null => Ok(CellValue::None),
                Value::Bool(v) => Ok(CellValue::Boolean(*v)),
                Value::Number(v) => v.as_f64().map(CellValue::Number).ok_or("Invalid function number".into()),
                Value::String(v) => Ok(CellValue::String(v.clone())),
                _ => Err("Application function returned a non-scalar value".into()),
            }
        }));
    }
    for (index, sheet) in sheets.iter().enumerate() {
        if index == 0 {
            model.rename_sheet_by_index(0, &sheet.name)?;
        } else {
            model.add_sheet(&sheet.name)?;
        }
    }
    for (name, formula) in &snapshot.names {
        model.new_defined_name(name, None, formula.strip_prefix('=').unwrap_or(formula))?;
    }
    for (index, sheet) in sheets.iter().enumerate() {
        for row in sheet.hidden_rows.iter().chain(&sheet.filtered_rows) {
            if !(1..=1_048_576).contains(row) { return Err("Hidden row exceeds worksheet bounds".into()); }
            model.set_row_hidden(index as u32, *row, true)?;
        }
        for row in &sheet.filtered_rows {
            model.filtered_rows.insert((index as u32, *row));
        }
        for cell in &sheet.cells {
            if let Some(result) = cancellation() { return Ok(result); }
            model.set_user_input(index as u32, cell.row, cell.col, cell.input.clone())?;
        }
    }
    if let Some(query) = &snapshot.affine_query {
        let addresses = |key: &str| -> Result<Vec<ironcalc_base::expressions::types::CellReferenceIndex>, String> {
            let entries=query.get(key).and_then(Value::as_array).ok_or("Invalid affine addresses")?;
            entries.iter().map(|entry| {
                let id=entry.get("sheet").and_then(Value::as_str).ok_or("Invalid affine worksheet")?;
                let sheet=sheets.iter().position(|sheet|sheet.id==id).ok_or("Affine worksheet was not found")?;
                let row=entry.get("row").and_then(Value::as_i64).ok_or("Invalid affine row")?;
                let column=entry.get("col").and_then(Value::as_i64).ok_or("Invalid affine column")?;
                if !(1..=1_048_576).contains(&row)||!(1..=16_384).contains(&column) {return Err("Invalid affine cell bounds".into());}
                Ok(ironcalc_base::expressions::types::CellReferenceIndex {sheet:sheet as u32,row:row as i32,column:column as i32})
            }).collect()
        };
        return Ok(match model.qualify_affine(&addresses("variables")?, &addresses("outputs")?) {
            Ok(())=>json!({"revision":snapshot.revision,"status":"affine","values":[],"diagnostics":[]}),
            Err(message)=>error_result(snapshot.revision.clone(),"unsupported_linear_model",&message),
        });
    }
    if let Some(result) = cancellation() { return Ok(result); }
    let iteration = if snapshot.settings.get("iterationEnabled").and_then(Value::as_bool) == Some(true) {
        let max_iterations = snapshot.settings.get("maxIterations").and_then(Value::as_u64).unwrap_or(100);
        let max_change = snapshot.settings.get("maxChange").and_then(Value::as_f64).unwrap_or(0.001);
        if max_iterations == 0 || max_iterations > 100_000 || !max_change.is_finite() || max_change < 0.0 {
            return Err("Iteration requires 1..100000 passes and a nonnegative finite tolerance".into());
        }
        let (passes, converged) = model.evaluate_iterative(max_iterations as u32, max_change);
        Some(json!({"iterations":passes,"converged":converged,"maxChange":max_change}))
    } else {
        model.evaluate();
        None
    };
    if let Some(result) = cancellation() { return Ok(result); }
    let mut values = Vec::new();
    let mut diagnostics = link_diagnostics;
    if iteration.as_ref().is_some_and(|result| result.get("converged") == Some(&Value::Bool(false))) {
        diagnostics.push(json!({"code":"iteration_not_converged","message":"Circular calculation did not converge within the configured limit"}));
    }
    let mut coordinates = BTreeMap::new();
    for (index, sheet) in snapshot.sheets.iter().enumerate() {
        for cell in &sheet.cells {
            coordinates.insert((index as u32, cell.row, cell.col), Some(cell.input.as_str()));
        }
    }
    for cell in model.get_all_cells() {
        if (cell.index as usize) < root_sheet_count {
            coordinates.entry((cell.index, cell.row, cell.column)).or_insert(None);
        }
    }
    for ((index, row, col), input) in coordinates {
            if let Some(result) = cancellation() { return Ok(result); }
            let sheet = &snapshot.sheets[index as usize];
            let computed = model.get_cell_value_by_index(index, row, col)?;
            let is_error = model.get_cell_type(index, row, col)? == CellType::ErrorValue;
            if snapshot.computed_results && !is_error && input.is_some_and(|input| reconstructible_literal(input, &computed)) {
                continue;
            }
            let (mut kind, value) = match computed {
                CellValue::Number(number) => ("number", json!(number)),
                CellValue::String(text) => ("text", json!(text)),
                CellValue::Boolean(boolean) => ("boolean", json!(boolean)),
                CellValue::None => ("empty", Value::Null),
            };
            if is_error {
                kind = "error";
                diagnostics.push(json!({"code":"formula_error","sheet":sheet.id,"row":row,"col":col,"message":value}));
            }
            values.push(json!({"sheet":sheet.id,"row":row,"col":col,"type":kind,"value":value}));
    }
    let converged = !iteration.as_ref().is_some_and(|result| result.get("converged") == Some(&Value::Bool(false)));
    let mut result = json!({"revision":snapshot.revision,"status":if converged { "ok" } else { "not_converged" },
        "values":if converged { values } else { Vec::new() },"diagnostics":diagnostics});
    if let Some(iteration) = iteration { result["iteration"] = iteration; }
    Ok(result)
}

fn error_result(revision: Value, code: &str, message: &str) -> Value {
    json!({"revision":revision,"status":code,"values":[],"diagnostics":[{"code":code,"message":message}]})
}

fn output(value: Value) -> *mut c_char {
    // JSON escapes embedded NUL characters, so a serialized value is a valid C string.
    CString::new(value.to_string()).expect("JSON contains no literal NUL").into_raw()
}

#[no_mangle]
pub extern "C" fn tigersheets_calc_abi_version() -> u32 { 1 }

#[no_mangle]
pub extern "C" fn tigersheets_calc_functions() -> *mut c_char {
    let catalog = catch_unwind(|| {
      let mut functions = registered_functions();
      functions.push("AGGREGATE".into()); functions.push("GETPIVOTDATA".into()); functions.push("FIELDVALUE".into()); functions.sort(); functions.dedup();
      json!({
        "engine":"IronCalc", "version":"v0.9.1",
        "commit":"eb9ca75bd99160c17902359d2d87225c4571c468",
        "availability_only":true, "functions":functions,"extensions":["AGGREGATE","GETPIVOTDATA","FIELDVALUE"],
        "bounded_functions":{"FIELDVALUE":{"linked_data_types":false,"non_rich_values":"#FIELD!"}}
      })
    });
    output(catalog.unwrap_or_else(|_| error_result(Value::Null, "internal_error", "Function catalog panicked")))
}

#[no_mangle]
pub extern "C" fn tigersheets_calc_new() -> *mut CalcHandle {
    Box::into_raw(Box::new(CalcHandle { cancelled: AtomicBool::new(false) }))
}

/// # Safety
/// `handle` must remain live throughout this thread-safe call.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_cancel(handle: *const CalcHandle) {
    if let Some(handle) = handle.as_ref() { handle.cancelled.store(true, Ordering::Release); }
}

/// # Safety
/// `handle` must remain live and its previous calculation must have ended.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_reset_cancel(handle: *const CalcHandle) {
    if let Some(handle) = handle.as_ref() { handle.cancelled.store(false, Ordering::Release); }
}

/// # Safety
/// `handle` must be a live pointer returned by `tigersheets_calc_new`, destroyed once.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_destroy(handle: *mut CalcHandle) {
    if !handle.is_null() { drop(Box::from_raw(handle)); }
}

/// # Safety
/// `handle` must remain live and `input` must be a NUL-terminated UTF-8 string.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_calculate(handle: *const CalcHandle, input: *const c_char) -> *mut c_char {
    tigersheets_calc_calculate_with_functions(handle, input, None, std::ptr::null_mut())
}

/// # Safety
/// All pointers follow `tigersheets_calc_calculate`; query is UTF-8 JSON with
/// variables and outputs arrays of {sheet,row,col} addresses.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_qualify_affine(handle: *const CalcHandle,input: *const c_char,query: *const c_char) -> *mut c_char {
    if input.is_null()||query.is_null() {return output(error_result(Value::Null,"invalid_snapshot","Null affine input"));}
    let request=(|| -> Result<CString,String> {
        let mut book:Value=serde_json::from_str(CStr::from_ptr(input).to_str().map_err(|e|e.to_string())?).map_err(|e|e.to_string())?;
        book["_affineQuery"]=serde_json::from_str(CStr::from_ptr(query).to_str().map_err(|e|e.to_string())?).map_err(|e|e.to_string())?;
        CString::new(book.to_string()).map_err(|e|e.to_string())
    })();
    match request {Ok(request)=>tigersheets_calc_calculate(handle,request.as_ptr()),Err(error)=>output(error_result(Value::Null,"invalid_snapshot",&error))}
}

/// # Safety
/// format and locale must remain live NUL-terminated UTF-8 strings for this call.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_format_number(value: f64,format: *const c_char,locale: *const c_char) -> *mut c_char {
    let result=catch_unwind(AssertUnwindSafe(|| -> Result<Value,String> {
        if !value.is_finite()||format.is_null()||locale.is_null() {return Err("Invalid number formatter input".into());}
        let format=CStr::from_ptr(format).to_str().map_err(|e|e.to_string())?;
        if format.len()>4096 {return Err("Number format exceeds 4096 bytes".into());}
        let locale=ironcalc_base::locale::get_locale(CStr::from_ptr(locale).to_str().map_err(|e|e.to_string())?)?;
        let result=ironcalc_base::formatter::format::format_number(value,format,locale);
        if let Some(error)=result.error {return Err(error);}
        Ok(json!({"status":"ok","text":result.text,"color":result.color}))
    }));
    output(match result {Ok(Ok(value))=>value,Ok(Err(error))=>json!({"status":"error","message":error}),Err(_)=>json!({"status":"error","message":"Number formatter failed"})})
}

/// # Safety
/// Inputs follow `tigersheets_calc_calculate`; callback responses must remain
/// live until consumed synchronously and must be NUL-terminated UTF-8 JSON.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_calculate_with_functions(handle: *const CalcHandle,
    input: *const c_char, callback: Option<FunctionCallback>, context: *mut c_void) -> *mut c_char {
    tigersheets_calc_calculate_with_references(handle,input,callback,None,context)
}

/// # Safety
/// Pointers and both synchronous callbacks follow the scalar callback contract.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_calculate_with_references(handle: *const CalcHandle,
    input: *const c_char,callback: Option<FunctionCallback>,reference_callback: Option<FunctionCallback>,context: *mut c_void) -> *mut c_char {
    let result = catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() || input.is_null() {
            return error_result(Value::Null, "invalid_snapshot", "Null calculation handle or input");
        }
        let text = match CStr::from_ptr(input).to_str() {
            Ok(value) => value,
            Err(_) => return error_result(Value::Null, "invalid_snapshot", "Snapshot must be UTF-8"),
        };
        // A typed streaming read skips styles, charts and retained report
        // caches instead of first allocating a second generic workbook tree.
        let snapshot: Snapshot = match serde_json::from_str(text) {
            Ok(value) => value,
            Err(_) => {
                // Keep the established error/revision and duplicate-field
                // behavior on the uncommon noncanonical or invalid path.
                let document:Value=match serde_json::from_str(text) {
                    Ok(value)=>value,
                    Err(error)=>return error_result(Value::Null,"invalid_snapshot",&error.to_string()),
                };
                let revision=document.get("revision").cloned().unwrap_or(Value::Null);
                match serde_json::from_value(document) {
                    Ok(value)=>value,
                    Err(error)=>return error_result(revision,"invalid_snapshot",&error.to_string()),
                }
            },
        };
        let revision = snapshot.revision.clone();
        match evaluate_snapshot(snapshot, &(*handle).cancelled, callback, reference_callback, context) {
            Ok(value) => value,
            Err(message) => error_result(revision, "invalid_snapshot", &message),
        }
    }));
    output(result.unwrap_or_else(|_| error_result(Value::Null, "internal_error", "Calculation engine panicked")))
}

/// # Safety
/// `text` must be a response allocated by this library, freed exactly once.
#[no_mangle]
pub unsafe extern "C" fn tigersheets_calc_free(text: *mut c_char) {
    if !text.is_null() { drop(CString::from_raw(text)); }
}

#[cfg(test)]
mod allocation_observer {
    use std::alloc::{GlobalAlloc, Layout, System};
    use std::cell::Cell;
    thread_local! {
        static ACTIVE: Cell<bool> = const { Cell::new(false) };
        static BYTES: Cell<usize> = const { Cell::new(0) };
    }
    pub struct Allocator;
    fn record(size: usize) { let _ = ACTIVE.try_with(|active| if active.get() {
        let _ = BYTES.try_with(|bytes| bytes.set(bytes.get().saturating_add(size)));
    }); }
    unsafe impl GlobalAlloc for Allocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 { record(layout.size()); System.alloc(layout) }
        unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 { record(layout.size()); System.alloc_zeroed(layout) }
        unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, size: usize) -> *mut u8 { record(size); System.realloc(pointer, layout, size) }
        unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) { System.dealloc(pointer, layout) }
    }
    pub fn start() { BYTES.with(|bytes| bytes.set(0)); ACTIVE.with(|active| active.set(true)); }
    pub fn finish() -> usize { ACTIVE.with(|active| active.set(false)); BYTES.with(Cell::get) }
}

#[cfg(test)]
#[global_allocator]
static OBSERVED_ALLOCATOR: allocation_observer::Allocator = allocation_observer::Allocator;

#[cfg(test)]
mod computed_result_tests {
    use super::*;

    fn calculate(document: &Value) -> Value {
        let input = CString::new(document.to_string()).unwrap();
        unsafe {
            let handle = tigersheets_calc_new();
            let response = tigersheets_calc_calculate(handle, input.as_ptr());
            let result = serde_json::from_str(CStr::from_ptr(response).to_str().unwrap()).unwrap();
            tigersheets_calc_free(response);
            tigersheets_calc_destroy(handle);
            result
        }
    }

    #[test]
    fn fieldvalue_non_rich_values_and_error_identity() {
        // Microsoft explicitly specifies #FIELD! for a value which is not a
        // linked data type. This fixture contains only native scalar cells.
        let document = json!({"revision": 91, "sheets": [{"id":"s", "name":"Sheet1", "cells":[
            {"row":1,"col":1,"input":"42"},
            {"row":2,"col":1,"input":"'Otis"},
            {"row":3,"col":1,"input":"TRUE"},
            {"row":1,"col":2,"input":"=FIELDVALUE(A1,\"Name\")"},
            {"row":2,"col":2,"input":"=FIELDVALUE(A2,\"Name\")"},
            {"row":3,"col":2,"input":"=FIELDVALUE(A3,\"Name\")"},
            {"row":4,"col":2,"input":"=FIELDVALUE(A4,\"Name\")"},
            {"row":5,"col":2,"input":"=_xlfn.FIELDVALUE(A1,\"Name\")"},
            {"row":6,"col":2,"input":"=IFERROR(B1,\"fallback\")"},
            {"row":7,"col":2,"input":"=ISERROR(B1)"},
            {"row":8,"col":2,"input":"=ERROR.TYPE(B1)"},
            {"row":9,"col":2,"input":"=TYPE(B1)"},
            {"row":10,"col":2,"input":"=B1+1"},
            {"row":11,"col":2,"input":"#FIELD!"},
            {"row":12,"col":2,"input":"=ERROR.TYPE(B11)"},
            {"row":13,"col":2,"input":"=ISERROR(#FIELD!)"},
            {"row":14,"col":2,"input":"=FIELDVALUE(1/0,\"Name\")"}
        ]}]});
        let result = calculate(&document);
        assert_eq!(result["status"], "ok");
        let at = |row: i64| result["values"].as_array().unwrap().iter()
            .find(|value| value["sheet"] == "s" && value["row"] == row && value["col"] == 2).unwrap();
        for row in [1, 2, 3, 4, 5, 10, 11] {
            assert_eq!(at(row)["type"], "error", "row {row} is an actual typed error");
            assert_eq!(at(row)["value"], "#FIELD!", "row {row} uses the distinct FIELD error");
        }
        assert_eq!(at(6)["value"], "fallback");
        assert_eq!(at(7)["value"], true);
        assert_eq!(at(8)["value"].as_f64(), Some(13.0));
        assert_eq!(at(9)["value"].as_f64(), Some(16.0));
        assert_eq!(at(12)["value"].as_f64(), Some(13.0));
        assert_eq!(at(13)["value"], true);
        assert_eq!(at(14)["value"], "#DIV/0!");
    }

    #[test]
    fn excel_final_subtraction_compensation() {
        // Microsoft documents the first expression as exact zero. The
        // independent 0.1+2.7+0.8 / 3.6 context examples are recorded at
        // learn.microsoft.com/answers/questions/5039407 and /5073937.
        let formulas = [
            "=1.333+1.225-1.333-1.225",
            "=A1*10000000000000000+5",
            "=(1.333+1.225-1.333-1.225)",
            "=SUM(1.333+1.225-1.333-1.225)",
            "=1.333+1.225-1.333-1.225-0",
            "=(43.1-43.2)+1",
            "=1E-12",
            "=1E-100",
            "=1E-16-0",
            "=0-1E-16",
            "=1E-16+1E-16",
            "=1.23456789-1.23456788",
        ];
        let mut cells: Vec<Value> = formulas.iter().enumerate().map(|(index, input)|
            json!({"row":index+1,"col":1,"input":input})).collect();
        for (column, input) in ["=0.1+2.7+0.8", "3.6", "=A20-B20", "=(A20-B20)",
            "=SUM(A20-B20)", "=IF(TRUE,A20-B20)", "=A20-B20-0", "=A20+-B20"].iter().enumerate() {
            cells.push(json!({"row":20,"col":column+1,"input":input}));
        }
        // Independently published Excel bit-level sweep and power-of-two
        // boundary: docs.grid.is/apiary/explain/excel-floating-point-arithmetic/.
        let boundaries = [
            ("=1+2^-49-1", 2.0_f64.powi(-49)),
            ("=1+2^-50-1", 0.0),
            ("=3300+2^-38-3300", 2.0_f64.powi(-38)),
            ("=3300+2^-39-3300", 0.0),
            ("=(2^53-9)-(2^53-10)", 0.0),
            ("=70368744177664-0.1-70368744177664", -0.1015625),
            ("=140737488355328+0.1-140737488355328", 0.0),
            ("=(140737488355328+0.1-140737488355328)", 0.09375),
            ("=SUM(1+2^-52-1)", 2.0_f64.powi(-52)),
            ("=LET(x,1+2^-52-1,x)", 2.0_f64.powi(-52)),
        ];
        for (index, (input, _)) in boundaries.iter().enumerate() {
            cells.push(json!({"row":30+index,"col":1,"input":input}));
        }
        let book = json!({"revision":75,"sheets":[{"id":"s","name":"Precision","cells":cells}]});
        let result = calculate(&book);
        assert_eq!(result["status"], "ok");
        let number = |row: u64, column: u64| -> f64 {
            result["values"].as_array().unwrap().iter().find(|cell|
                cell["row"] == row && cell["col"] == column).unwrap()["value"].as_f64().unwrap()
        };
        assert_eq!(number(1, 1), 0.0, "Microsoft's near-zero final-subtraction example must be exact zero");
        assert_eq!(number(2, 1), 5.0, "dependent formulas must consume the corrected stored value");
        let raw = ((1.333_f64 + 1.225) - 1.333) - 1.225;
        assert_ne!(raw, 0.0);
        for row in [3, 4, 5] {
            assert_eq!(number(row, 1).to_bits(), raw.to_bits(), "grouped, nested and final-zero-operand expressions retain raw arithmetic");
        }
        assert_eq!(number(6, 1).to_bits(), ((43.1_f64 - 43.2) + 1.0).to_bits());
        for (row, expected) in [(7, 1e-12_f64), (8, 1e-100), (9, 1e-16), (10, -1e-16), (11, 2e-16), (12, 1.23456789_f64-1.23456788)] {
            assert_eq!(number(row, 1).to_bits(), expected.to_bits(), "genuine small values must not be globally rounded or clamped");
        }
        let independent = (0.1_f64 + 2.7 + 0.8) - 3.6;
        assert_eq!(number(20, 3), 0.0);
        assert_eq!(number(20, 8), 0.0, "root addition with an opposite-sign operand shares cancellation semantics");
        for column in [4, 5, 6, 7] {
            assert_eq!(number(20, column).to_bits(), independent.to_bits(), "only the final arithmetic operation receives compensation");
        }
        for (index, (_, expected)) in boundaries.iter().enumerate() {
            assert_eq!(number((30+index) as u64, 1).to_bits(), expected.to_bits(), "published Excel exponent-boundary case {}", index);
        }
        assert_eq!(book["sheets"][0]["cells"][0]["input"], formulas[0], "calculation does not rewrite canonical formulas");
    }

    #[test]
    fn typed_input_retains_revision_and_ignores_noncalculation_metadata() {
        let base=json!({"revision":73,"sheets":[{"id":"s","name":"Data","cells":[{"row":1,"col":1,"input":"=2+3"}]}]});
        let mut decorated=base.clone();decorated["sheets"][0]["cells"][0]["style"]=json!({"numberFormat":"$#,##0.00"});decorated["charts"]=json!([{"opaque":true}]);
        assert_eq!(calculate(&base),calculate(&decorated));
        decorated["sheets"][0]["cells"][0]["row"]=json!("invalid");
        let refused=calculate(&decorated);assert_eq!(refused["revision"],73);assert_eq!(refused["status"],"invalid_snapshot");
    }

    #[test]
    fn ignored_metadata_does_not_allocate_a_second_workbook_tree() {
        let metadata:Vec<Value>=(0..10000).map(|_|json!({"style":{"fill":"#e7efe6","font":"DejaVu Sans","rows":[1,2,3,4]},"preservedPayload":"x".repeat(128)})).collect();
        let book=json!({"revision":74,"sheets":[{"id":"s","name":"Data","cells":[{"row":1,"col":1,"input":"=2+3"}]}],"unusedWorkbookMetadata":metadata});
        let input=CString::new(book.to_string()).unwrap();unsafe {
            let handle=tigersheets_calc_new();allocation_observer::start();let response=tigersheets_calc_calculate(handle,input.as_ptr());let bytes=allocation_observer::finish();
            let result:Value=serde_json::from_str(CStr::from_ptr(response).to_str().unwrap()).unwrap();tigersheets_calc_free(response);tigersheets_calc_destroy(handle);
            assert_eq!(result["status"],"ok");assert!(bytes<2_000_000,"ignored metadata allocated {bytes} bytes instead of being skipped");
        }
    }

    #[test]
    fn computed_results_omit_only_exactly_reconstructible_literals() {
        let literals = ["42", "-1.25", "1e3", "00123", "'00123", "Hello", "TRUE", "FALSE", "", "'",
            "+1", "10%", "1/1/2024", "true", "false", " 42", "42 ", "#DIV/0!", "NaN", "inf", "-inf", "=42"];
        let mut cells: Vec<Value> = literals.iter().enumerate().map(|(index, input)|
            json!({"row":index + 1,"col":1,"input":input})).collect();
        cells.push(json!({"row":40,"col":1,"input":"=SEQUENCE(2,1,5)"}));
        let mut book = json!({"revision":8,"sheets":[{"id":"s","name":"Sheet1","cells":cells}]});
        let full = calculate(&book);
        assert_eq!(full["status"], "ok");
        assert_eq!(full["values"].as_array().unwrap().len(), literals.len() + 2);
        book["_tigersheetsComputedResults"] = json!(true);
        let sparse = calculate(&book);
        let retained = sparse["values"].as_array().unwrap();
        assert!(retained.len() < full["values"].as_array().unwrap().len(),
            "opt-in computed results must omit exactly reconstructible literal values");
        for row in 1..=10 {
            assert!(!retained.iter().any(|cell| cell["row"] == row), "ordinary literal row {row} must use raw fallback");
        }
        for row in [11,12,13,14,15,18,22,40,41] {
            let cell = retained.iter().find(|cell| cell["row"] == row)
                .unwrap_or_else(|| panic!("exceptional literal/formula/spill row {row} must be retained"));
            assert!(full["values"].as_array().unwrap().contains(cell));
        }
        assert_eq!(sparse["diagnostics"], full["diagnostics"]);
        assert_eq!(sparse["status"], full["status"]);
        book["_tigersheetsComputedResults"] = json!(false);
        assert_eq!(calculate(&book), full, "default/full response remains unchanged");
    }

    #[test]
    fn number_format_named_colors_use_the_same_palette_indices_as_color_n() {
        let locale = ironcalc_base::locale::get_locale("en").unwrap();
        for (index, name) in ["Black", "White", "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan"].iter().enumerate() {
            let named = ironcalc_base::formatter::format::format_number(-5.0, &format!("0;[{name}](0)"), locale);
            let indexed = ironcalc_base::formatter::format::format_number(-5.0, &format!("0;[Color {}](0)", index + 1), locale);
            assert_eq!(named.error, None, "named numeric color {name} is valid");
            assert_eq!(named.text, "(5)");
            assert_eq!(named.color, Some((index + 1) as i32), "named color {name} uses its one-based spreadsheet palette index");
            assert_eq!(named.color, indexed.color, "named and indexed forms choose the same numeric color");
        }
    }

    #[test]
    fn computed_results_preserve_failed_calculation_contracts() {
        let mut book = json!({"revision":9,"settings":{"iterationEnabled":true,"maxIterations":1,"maxChange":0},
            "sheets":[{"id":"s","name":"Sheet1","cells":[{"row":1,"col":1,"input":"=A1+1"}]}]});
        let full = calculate(&book);
        assert_eq!(full["status"], "not_converged");
        book["_tigersheetsComputedResults"] = json!(true);
        assert_eq!(calculate(&book), full);
        let input = CString::new(book.to_string()).unwrap();
        unsafe {
            let handle = tigersheets_calc_new();
            tigersheets_calc_cancel(handle);
            let response = tigersheets_calc_calculate(handle, input.as_ptr());
            let cancelled: Value = serde_json::from_str(CStr::from_ptr(response).to_str().unwrap()).unwrap();
            tigersheets_calc_free(response);
            tigersheets_calc_destroy(handle);
            assert_eq!(cancelled["status"], "cancelled");
            assert_eq!(cancelled["values"], json!([]));
            assert_eq!(cancelled["diagnostics"][0]["code"], "cancelled");
        }
    }
}
