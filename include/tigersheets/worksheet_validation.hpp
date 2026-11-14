#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace tigersheets {
using Json=nlohmann::json;
// Target coordinates are inclusive and 1-based. List sources are bounded A1
// rows or columns, optionally qualified with a workbook worksheet name.
Json makeWorksheetListValidation(const Json& workbook,const std::string& sheetId,const Json& range,const std::string& formula1,bool allowBlank=true);
void validateWorksheetValidations(const Json& workbook,const Json& sheet);
const Json* worksheetListValidationAt(const Json& sheet,int row,int col);
// Each option contains label, input, type and the original typed value.
// Formula source values must come from a current successful calculation.
Json worksheetValidationOptions(const Json& workbook,const Json& calculated,const std::string& sheetId,const Json& validation);
}
