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
// Save formula caches from a complete successful calculation of this revision.
// Cache validation and package writing are one atomic save transaction.
void writeWorkbookCalculated(const std::string& path,const Json& workbook,const Json& calculation,WorkbookPackage* source=nullptr);
// A host can supply explicitly prepared live/retained source snapshots so that
// native external-link caches reflect unsaved open workbook edits on save.
void writeWorkbook(const std::string& path,const Json& workbook,WorkbookPackage* source,const Json& resolvedLinks);
void writeWorkbookCalculated(const std::string& path,const Json& workbook,const Json& calculation,WorkbookPackage* source,const Json& resolvedLinks);
}
