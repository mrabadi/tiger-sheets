#pragma once
#include "tigersheets/session.hpp"
#include <QString>
#include <functional>
class QTableView;
namespace tigersheets {
class WorksheetModel;
void installWorksheetContextMenus(QTableView& view,WorksheetModel& model,
 std::function<bool(const QString&,const Json&)> execute);
void invalidateWorksheetContextMenus(QTableView& view);
}
