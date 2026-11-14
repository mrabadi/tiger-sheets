#pragma once
#include "tigersheets/pivot.hpp"
namespace tigersheets {
// Validate legacy metadata and produce a small requested native definition.
// Does not calculate or copy worksheet cells/native caches; the caller supplies
// a fresh destination worksheet before submitting this definition to a worker.
Json staticPivotConversionDefinition(const Json& workbook,const std::string& legacyId,
 const std::string& newSheetId,const std::string& newPivotId,const std::string& newPivotName);
// Explicitly convert supported static-summary metadata into a native report on a
// new worksheet. The old summary cells remain ordinary cells, without mutation.
// Returned operations are one revision-bound, previewable transaction.
Json convertStaticPivotOperations(const Json& workbook,const Json& calculated,
 const std::string& legacyId,const std::string& newSheetId,const std::string& newSheetName,
 const std::string& newPivotId,const std::string& newPivotName,
 std::uint64_t contextGeneration=0,const std::function<bool()>& cancelled={});
}
