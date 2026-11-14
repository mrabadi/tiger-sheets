#pragma once
#include "tigersheets/session.hpp"
#include <string_view>
namespace tigersheets {
using CsvRows=std::vector<std::vector<std::string>>;
CsvRows parseCsv(std::string_view text);
std::string formatCsv(const CsvRows& rows);
Json readCsv(const std::string& path);
// Isolated CSV-only values copy; pass the same selected sheetId to writeCsv.
Json csvValuesSnapshot(const Json& workbook,const Json& calculated,const std::string& sheetId={});
void writeCsv(const std::string& path,const Json& workbook,const std::string& sheetId="");
}
