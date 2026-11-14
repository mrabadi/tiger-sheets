#pragma once
#include "tigersheets/ooxml.hpp"
namespace tigersheets {
void readNativePrintSettings(WorkbookPackage& package);
void writeNativePrintSettings(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& sheetParts);
bool supportedNativePrintName(const WorkbookPackage& source,const std::string& name,const std::string& localSheetId,const std::string& formula);
}
