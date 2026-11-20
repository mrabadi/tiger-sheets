#pragma once
#include "tigersheets/session.hpp"
class QWidget;
class QTableView;
namespace tigersheets {
class WorksheetModel;
// Validate and normalize a patch before batching distinct per-cell changes.
Json validatedWorksheetCellStyle(const Json& style);
// Merge per-side border patches and their opposite neighbor edges into one
// atomic set of cell style operations. Null/None edges mean explicit removal.
// Full replace operations pass through; callers clear shared edges separately.
Json worksheetBorderPatchOperations(const WorksheetModel& model,const Json& operations);
double worksheetFontDefaultRowHeight(const QTableView& view,const Json& effectiveStyle);
// Without style arguments, show the native Format Cells dialog.
bool formatWorksheetCells(QWidget* parent,WorksheetModel& model,QTableView& view,const Json& arguments=Json::object());
bool formatWorksheetBorders(WorksheetModel& model,QTableView& view,const Json& arguments);
bool clearWorksheetFormats(WorksheetModel& model,QTableView& view);
}
