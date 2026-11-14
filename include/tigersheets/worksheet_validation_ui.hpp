#pragma once
#include "tigersheets/session.hpp"
#include <QObject>
#include <functional>
#include <memory>
class QWidget;
class QTableView;
class QTabBar;
namespace tigersheets {
class WorksheetModel;
class WorksheetValidationUi : public QObject {
public:
 WorksheetValidationUi(QWidget*,WorksheetModel&,QTableView&,QTabBar&,std::function<Json()>);
 ~WorksheetValidationUi() override;
 bool edit(const Json& arguments=Json::object());
 void reset();
protected:
 bool eventFilter(QObject*,QEvent*) override;
private:
 struct State;
 std::unique_ptr<State> state_;
 void refresh();
 void openOptions();
 void pickSource();
};
}
