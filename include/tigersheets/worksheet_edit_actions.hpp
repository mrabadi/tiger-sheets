#pragma once
#include "tigersheets/session.hpp"
#include <QString>
class QWidget;
class QTableView;
namespace tigersheets {
class WorksheetModel;
struct WorkbookPackage;
bool executeWorksheetEdit(QWidget* parent,WorksheetModel& model,QTableView& view,
 const QString& command,const Json& arguments,const WorkbookPackage* package=nullptr);
}
