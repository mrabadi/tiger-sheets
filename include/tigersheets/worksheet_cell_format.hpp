#pragma once
#include "tigersheets/session.hpp"
class QWidget;
class QTableView;
namespace tigersheets {
class WorksheetModel;
// Validate and normalize a patch before batching distinct per-cell changes.
Json validatedWorksheetCellStyle(const Json& style);
double worksheetFontDefaultRowHeight(const QTableView& view,const Json& effectiveStyle);
// Without style arguments, show the native Format Cells dialog.
bool formatWorksheetCells(QWidget* parent,WorksheetModel& model,QTableView& view,const Json& arguments=Json::object());
bool formatWorksheetBorders(WorksheetModel& model,QTableView& view,const Json& arguments);
bool clearWorksheetFormats(WorksheetModel& model,QTableView& view);
}
