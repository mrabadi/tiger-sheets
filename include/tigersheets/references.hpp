#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <string_view>
namespace tigersheets {
constexpr int MaxRows=1048576, MaxColumns=16384;
// Source spans are UTF-8 byte offsets; rectangle bounds are inclusive and 1-based.
struct FormulaReference {
 std::size_t start=0,length=0;
 int row1=1,col1=1,row2=1,col2=1;
 // Decoded sheet name; empty means the formula's own sheet. Workbook prefixes are omitted.
 std::string sheet;
 bool external=false;
};
std::vector<FormulaReference> formulaReferences(std::string_view formula);
std::string columnName(int column);
int columnNumber(std::string_view letters);
std::string shiftFormula(std::string_view formula,int deltaRow,int deltaColumn);
// Structural edits move references regardless of absolute-reference locks.
// The edited interval is 1-based; deleted references produce #REF!.
std::string editFormulaReferences(std::string_view formula,std::string_view formulaSheet,std::string_view editedSheet,bool rows,int index,int count,bool remove);
// Refuses partially intersected ranges instead of silently changing their meaning.
std::string moveFormulaReferences(std::string_view formula,std::string_view formulaSheet,std::string_view sourceSheet,std::string_view destinationSheet,int row1,int col1,int row2,int col2,int deltaRow,int deltaColumn,bool formulaMoved=false);
std::string cycleReference(std::string_view formula,std::size_t caret);
std::string renameSheetReferences(std::string_view formula,std::string_view oldName,std::string_view newName);
}
