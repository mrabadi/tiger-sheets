#pragma once
#include "tigersheets/session.hpp"
#include <functional>
class QWidget;
class QDialog;
class QTableView;
namespace tigersheets {
class WorksheetModel;
QDialog* showWorksheetFindReplace(QWidget* parent,WorksheetModel& model,QTableView& view,bool replace=false,std::function<Json()> calculation={});
}
