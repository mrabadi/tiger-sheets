#pragma once
#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
class QWidget;
class QTableView;
namespace tigersheets {
// Empty arguments open native dialogs. Supplied arguments provide the same actions
// for deterministic automation. Invalid requests throw; cancellation returns false.
bool modelingCommand(QWidget* parent,WorkbookSession& session,WorksheetModel& model,
 QTableView& view,const QString& command,const Json& args=Json::object(),const CalculationContext* context=nullptr);
}
