#pragma once
#include "tigersheets/ooxml.hpp"
namespace tigersheets {
// Native workbook/worksheet view metadata is separate from cell edit history.
void readNativeWorkbookViews(WorkbookPackage& package);
void writeNativeWorkbookViews(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& parts);
}
