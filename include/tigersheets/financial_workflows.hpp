#pragma once
#include "tigersheets/calculation.hpp"
namespace tigersheets {
// Computes a reviewable operation batch; it never applies or saves the workbook.
Json executeFinancialWorkflow(const Json& workbook,const Json& definition,
 const std::string& action,const CalculationContext& context,
 const std::function<bool()>& cancelled={});
}
