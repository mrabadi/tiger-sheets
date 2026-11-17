#pragma once
#include <nlohmann/json.hpp>
#include <string_view>
namespace tigersheets {
// Qt::UserRole + 60, kept independent of Qt for the calculation library.
inline constexpr int FormulaErrorCheckRole=0x013c;
// Intrinsic input checks remain available while calculation is pending.
bool numberStoredAsTextWarning(const nlohmann::json& sheet,int row,int col,std::string_view input);
// Conservative numeric SUM-range omission checks; never modifies authored data.
nlohmann::json formulaErrorChecks(const nlohmann::json& snapshot,const nlohmann::json& calculation);
}
