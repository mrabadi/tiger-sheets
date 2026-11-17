use crate::{
    calc_result::CalcResult,
    expressions::{parser::Node, token::Error, types::CellReferenceIndex},
    model::Model,
};

impl Model<'_> {
    pub(crate) fn fn_fieldvalue(&mut self, args: &[Node], cell: CellReferenceIndex) -> CalcResult {
        if args.len() != 2 {
            return CalcResult::new_args_number_error(cell);
        }
        for argument in args {
            let result = self.evaluate_node_in_context(argument, cell);
            if result.is_error() {
                return result;
            }
        }
        // Native values currently contain no linked/compound records. Microsoft
        // specifies #FIELD! when the supplied value is not a linked data type.
        // Keep a real error so ISERROR, IFERROR and ERROR.TYPE remain meaningful.
        CalcResult::new_error(
            Error::FIELD,
            cell,
            "The value is not a linked data type".to_string(),
        )
    }
}
