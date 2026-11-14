#pragma once
#include "tigersheets/session.hpp"
namespace tigersheets {
// Convert dialog character counts, raw SpreadsheetML col/@width and logical
// pixels using the Normal-style maximum digit width. Authored raw widths cap at 255.
double spreadsheetColumnWidth(double characters,int maxDigitWidth);
int spreadsheetColumnPixels(double storedWidth,int maxDigitWidth);
double spreadsheetColumnCharacters(double storedWidth,int maxDigitWidth);
// Sparse dimensions use row points and raw SpreadsheetML column widths (padding included).
void validateWorksheetLayout(const Json& workbook,const Json& sheet);
void applyWorksheetLayout(Json& workbook,const Json& operation);
void rebaseWorksheetLayout(Json& sheet,bool rows,int index,int count,bool inserting);
}
