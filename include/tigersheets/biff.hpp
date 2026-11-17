#pragma once
#include "tigersheets/session.hpp"
#include <string_view>
namespace tigersheets {
bool isCompoundWorkbook(std::string_view bytes) noexcept;
Json readBiffWorkbook(std::string_view bytes);
struct WorkbookPackage;
void writeBiffWorkbook(const std::string& path,const Json& workbook,WorkbookPackage* source=nullptr);
}
