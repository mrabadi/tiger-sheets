#pragma once

#include "tigersheets/calculation.hpp"

namespace tigersheets {
Json normalizeSolverSpecification(const Json& snapshot,const Json& specification,const CalculationContext* context=nullptr);
Json solveWorkbook(const Json& snapshot,const Json& specification,const CalculationContext* context=nullptr,const std::function<bool()>& cancelled={});
}
