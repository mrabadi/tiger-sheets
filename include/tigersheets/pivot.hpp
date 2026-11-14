#pragma once
#include "tigersheets/session.hpp"
#include <functional>
namespace tigersheets {
struct PivotGeneration { Json definition; Json cells; };
struct PivotSourceCache { Json fields; Json records; };
// Extract a validated immutable source; callers bind the resulting cache to revision/context.
PivotSourceCache buildPivotSourceCache(const Json& workbook,const Json& calculated,const Json& definition,
 const std::function<bool()>& cancelled={});
// Calculation values and source must describe the same immutable revision.
PivotGeneration generatePivot(const Json& workbook,const Json& calculated,const Json& definition,
 std::uint64_t contextGeneration=0,const std::function<bool()>& cancelled={});
PivotGeneration generatePivotFromCache(const Json& workbook,const Json& definition,Json fields,Json records,
 std::uint64_t contextGeneration=0,const std::function<bool()>& cancelled={});
Json pivotOperations(const Json& workbook,const PivotGeneration& generation);
Json deletePivotOperations(const Json& workbook,const std::string& id);
Json pivotChart(const Json& workbook,const std::string& pivotId,const std::string& chartId,
 const std::string& type="column");
Json pivotChartData(const Json& workbook,const Json& chart);
Json getPivotData(const Json& workbook,const std::string& sheet,int row,int col,
 const std::string& measure,const Json& fieldItems=Json::array());
// Complete formula for one generated measure cell; empty for labels/ordinary cells.
std::string pivotReferenceFormula(const Json& workbook,const std::string& sheet,int row,int col);
}
