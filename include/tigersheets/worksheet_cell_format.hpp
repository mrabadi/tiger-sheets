#pragma once
#include "tigersheets/session.hpp"
class QWidget;
class QTableView;
namespace tigersheets {
class WorksheetModel;
// Without style arguments, show the native Format Cells dialog.
bool formatWorksheetCells(QWidget* parent,WorksheetModel& model,QTableView& view,const Json& arguments=Json::object());
bool clearWorksheetFormats(WorksheetModel& model,QTableView& view);
}
