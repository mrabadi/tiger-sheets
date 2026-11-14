//! TigerSheets-owned AGGREGATE worksheet function extension.
use crate::{
    calc_result::CalcResult,
    expressions::{parser::{ArrayNode, Node}, token::Error, types::CellReferenceIndex},
    functions::Function,
    model::Model,
};

fn contains_aggregate(node: &Node) -> bool {
    match node {
        Node::FunctionKind { kind: Function::Subtotal, .. } => true,
        Node::NamedFunctionKind { name, .. } if name.eq_ignore_ascii_case("AGGREGATE") => true,
        Node::FunctionKind { args, .. } | Node::NamedFunctionKind { args, .. } => args.iter().any(contains_aggregate),
        Node::OpRangeKind { left, right } | Node::OpConcatenateKind { left, right } |
        Node::OpSumKind { left, right, .. } | Node::OpProductKind { left, right, .. } |
        Node::OpPowerKind { left, right } | Node::CompareKind { left, right, .. } =>
            contains_aggregate(left) || contains_aggregate(right),
        Node::UnaryKind { right, .. } => contains_aggregate(right),
        Node::ImplicitIntersection { child, .. } | Node::SpillRangeOperator { child } => contains_aggregate(child),
        _ => false,
    }
}

fn append_value(value: CalcResult, ignore_errors: bool, function: i32,
    values: &mut Vec<ArrayNode>, nonempty: &mut usize) -> Result<(), CalcResult> {
    match value {
        CalcResult::Number(number) => { values.push(ArrayNode::Number(number)); *nonempty += 1; }
        CalcResult::EmptyCell | CalcResult::EmptyArg => {}
        CalcResult::Error { .. } if ignore_errors => {}
        CalcResult::Error { .. } if function == 2 || function == 3 => { *nonempty += 1; }
        error @ CalcResult::Error { .. } => return Err(error),
        CalcResult::String(_) | CalcResult::Boolean(_) => { *nonempty += 1; }
        _ => {}
    }
    Ok(())
}

impl<'a> Model<'a> {
    pub(crate) fn fn_aggregate(&mut self, args: &[Node], cell: CellReferenceIndex) -> CalcResult {
        if !(3..=255).contains(&args.len()) {
            return CalcResult::new_error(Error::VALUE, cell, "AGGREGATE requires function, options and references".into());
        }
        let function = match self.get_number(&args[0], cell) { Ok(v) => v.trunc() as i32, Err(e) => return e };
        let options = match self.get_number(&args[1], cell) { Ok(v) => v.trunc() as i32, Err(e) => return e };
        if !(1..=19).contains(&function) || !(0..=7).contains(&options) || (function >= 14 && args.len() != 4) {
            return CalcResult::new_error(Error::VALUE, cell, "Invalid AGGREGATE function, options or k".into());
        }
        let ignore_errors = matches!(options, 2 | 3 | 6 | 7);
        let ignore_hidden = matches!(options, 1 | 3 | 5 | 7);
        let ignore_nested = options <= 3;
        let data = if function >= 14 { &args[2..3] } else { &args[2..] };
        let mut values = Vec::new();
        let mut nonempty = 0;
        for argument in data {
            if ignore_nested && contains_aggregate(argument) { continue; }
            match self.evaluate_node_with_reference(argument, cell) {
                CalcResult::Range { left, right } => {
                    if left.sheet != right.sheet {
                        return CalcResult::new_error(Error::VALUE, cell, "AGGREGATE does not accept 3-D references".into());
                    }
                    let (last_row, last_column) = match self.clip_to_used_area(left.sheet, left.row, left.column, right.row, right.column) {
                        Ok(bounds) => bounds,
                        Err(message) => return CalcResult::new_error(Error::REF, cell, message),
                    };
                    for row in left.row..=last_row {
                        if self.filtered_rows.contains(&(left.sheet, row)) { continue; }
                        if ignore_hidden && self.is_row_hidden(left.sheet, row).unwrap_or(false) { continue; }
                        for column in left.column..=last_column {
                            if ignore_nested {
                                let nested = self.workbook.worksheets[left.sheet as usize].cell(row, column)
                                    .and_then(|value| value.get_formula())
                                    .map(|formula| contains_aggregate(&self.parsed_formulas[left.sheet as usize][formula as usize].0))
                                    .unwrap_or(false);
                                if nested { continue; }
                            }
                            let value = self.evaluate_cell(CellReferenceIndex {sheet:left.sheet,row,column});
                            if let Err(error) = append_value(value, ignore_errors, function, &mut values, &mut nonempty) { return error; }
                        }
                    }
                }
                CalcResult::Array(array) => {
                    for element in array.into_iter().flatten() {
                        let value = match element {
                            ArrayNode::Number(v) => CalcResult::Number(v), ArrayNode::String(v) => CalcResult::String(v),
                            ArrayNode::Boolean(v) => CalcResult::Boolean(v), ArrayNode::Empty => CalcResult::EmptyCell,
                            ArrayNode::Error(error) => CalcResult::new_error(error, cell, String::new()),
                        };
                        if let Err(error) = append_value(value, ignore_errors, function, &mut values, &mut nonempty) { return error; }
                    }
                }
                value => if let Err(error) = append_value(value, ignore_errors, function, &mut values, &mut nonempty) { return error; },
            }
        }
        if function == 2 { return CalcResult::Number(values.len() as f64); }
        if function == 3 { return CalcResult::Number(nonempty as f64); }
        if function == 6 {
            if values.is_empty() { return CalcResult::Number(0.0); }
            let operands: Vec<Node> = values.into_iter().filter_map(|value| match value {
                ArrayNode::Number(number) => Some(Node::NumberKind(number)), _ => None,
            }).collect();
            return self.fn_product(&operands, cell);
        }
        let mut operands = vec![Node::ArrayKind(vec![values])];
        if function >= 14 { operands.push(args[3].clone()); }
        match function {
            1 => self.fn_average(&operands, cell), 4 => self.fn_max(&operands, cell),
            5 => self.fn_min(&operands, cell), 6 => self.fn_product(&operands, cell),
            7 => self.fn_stdev_s(&operands, cell), 8 => self.fn_stdev_p(&operands, cell),
            9 => self.fn_sum(&operands, cell), 10 => self.fn_var_s(&operands, cell),
            11 => self.fn_var_p(&operands, cell), 12 => self.fn_median(&operands, cell),
            13 => self.fn_mode_sngl(&operands, cell), 14 => self.fn_large(&operands, cell),
            15 => self.fn_small(&operands, cell), 16 => self.fn_percentile_inc(&operands, cell),
            17 => self.fn_quartile_inc(&operands, cell), 18 => self.fn_percentile_exc(&operands, cell),
            19 => self.fn_quartile_exc(&operands, cell), _ => unreachable!(),
        }
    }
}
