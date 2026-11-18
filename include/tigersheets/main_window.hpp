#pragma once
#include "tigersheets/session.hpp"
#include <QMainWindow>
#include <memory>
#include <functional>
namespace tigersheets {
struct CalculationContext;
class MainWindow : public QMainWindow {
public:
 explicit MainWindow(QWidget* parent=nullptr);
 ~MainWindow() override;
 WorkbookSession& currentSession();
 void recalculateNow();
 bool executeCommand(const QString& command,const Json& arguments=Json::object());
 bool openWorkbook(const QString& path);
 bool saveWorkbook(const QString& path);
 void loadFinancialExample();
 QString lastError() const;
protected:
 bool eventFilter(QObject* object,QEvent* event) override;
 void closeEvent(QCloseEvent* event) override;
private:
 void resetWorkbookContext();
 void queueCalculation();
 void updateSelectionStatus();
 void displayCalculation(Json result);
 void positionCharts();
 void refreshChartPresentation(const Json& change);
 void commitEditor();
 Json calculationSnapshot() const;
 CalculationContext calculationContext(bool allowCachedLinks=false) const;
 bool reportingCommand(const QString& command,const Json& arguments);
 bool startReport(Json definition,bool useCachedLinks=false);
 void showPivotFields(const Json& definition);
 void showFinancialFields(const std::string& kind);
 bool startFinancialModel(const Json& definition,const std::string& action,bool allowCachedLinks=false);
 void startCodexTool(QString tool,Json arguments,std::function<void(Json)> completion);
 void cancelCodexTool();
 struct State;
 std::unique_ptr<State> state_;
};
}
