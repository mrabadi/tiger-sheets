#pragma once
#include <QString>
#include "tigersheets/session.hpp"
class QWidget;
class QTableView;
namespace tigersheets {
class WorksheetModel;
// A nonempty path exports a PDF. An empty path opens the native print dialog.
bool printWorksheet(QWidget* parent,WorksheetModel& model,QTableView& view,const QString& pdfPath={},const Json& workbook=Json::object(),const Json& calculated=Json::object());
}
