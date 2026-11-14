#pragma once
#include "tigersheets/calculation.hpp"

namespace tigersheets {
struct FinancialModelGeneration { Json definition; Json cells; };
FinancialModelGeneration generateFinancialModel(const Json& workbook,const Json& requested,
 const CalculationContext& context,const std::function<bool()>& cancelled={});
Json financialModelOperations(const Json& workbook,const FinancialModelGeneration& generation);
Json saveFinancialDefinitionOperations(const Json& workbook,const Json& requested);
Json applyFinancialScenarioOperations(const Json& workbook,const std::string& id);
Json deleteFinancialModelOperations(const Json& workbook,const std::string& id);
void applyFinancialModelTransaction(Json& workbook,const Json& operation);
void validateFinancialModels(const Json& workbook);
void validateFinancialModelEdit(const Json& workbook,const Json& operation);
void invalidateFinancialModels(Json& workbook,const Json& operation);
}
