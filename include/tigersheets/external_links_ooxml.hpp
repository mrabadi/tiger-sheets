#pragma once
#include "tigersheets/session.hpp"
#include <map>
namespace tigersheets {
// Returns augmented canonical link metadata, or null when no links are authored.
// Resolved sources are immutable calculation inputs supplied by the host; this
// serializer never opens linked files implicitly.
Json writeNativeExternalLinks(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& sheetParts,const Json* resolvedLinks=nullptr);
Json writeNativeExternalLinks(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& sheetParts,const Json* resolvedLinks,const std::string& savePath,const std::string& originalWorkbookPath);
void restoreNativeExternalFormulaNames(Json& workbook);
}
