#pragma once
#include "tigersheets/session.hpp"
#include <map>
namespace tigersheets {
struct WorkbookPackage {
 Json workbook;
 std::map<std::string,std::string> entries,sheetParts;
 Json baseline;
 std::string sourcePath,sourceHash;
};
WorkbookPackage readWorkbook(const std::string& path);
// Refuse coordinate edits before mutation when native objects cannot be safely rebased.
void preflightWorksheetStructure(const WorkbookPackage& source,const std::string& sheetId);
void preflightWorksheetLayout(const WorkbookPackage& source,const std::string& sheetId,const Json& mergedRanges);
void writeWorkbook(const std::string& path,const Json& workbook,WorkbookPackage* source=nullptr);
}
