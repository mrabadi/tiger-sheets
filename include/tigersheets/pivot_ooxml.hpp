#pragma once
#include "tigersheets/ooxml.hpp"
#include <set>
namespace tigersheets {
void writeNativePivots(std::map<std::string,std::string>& entries,const Json& workbook,
 const std::map<std::string,std::string>& sheetParts,const WorkbookPackage* source=nullptr);
void readNativePivots(WorkbookPackage& package);
bool ownedNativePivotPart(const std::string& xml);
std::set<std::string> ownedNativePivotParts(const WorkbookPackage& source);
std::string nativePivotChartXml(const Json& workbook,const Json& chart,const std::string& path);
}
