#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QTabBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
WorksheetModel& model(MainWindow& window){auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid!=nullptr,"desktop worksheet exists");auto* result=dynamic_cast<WorksheetModel*>(grid->model());require(result!=nullptr,"desktop worksheet model exists");return *result;}
void compactManualCalculation(){
 MainWindow window;auto& worksheet=model(window);Json cells=Json::array();for(int row=1;row<=100;++row)cells.push_back({{"row",row},{"col",1},{"input",row==1?"42":row==2?"=A1*2":row==3?"25%":row==4?"'00123":row==5?"TRUE":row==6?"#DIV/0!":row==7?"1/1/2024":"1"}});window.currentSession()=WorkbookSession({{"revision",5},{"sheets",Json::array({{{"id","s"},{"name","Data"},{"cells",std::move(cells)}}})}});worksheet.setSheetId("s");auto* grid=window.findChild<QTableView*>("worksheet.grid");grid->setCurrentIndex(worksheet.index(0,0));grid->selectionModel()->select(QItemSelection(worksheet.index(0,0),worksheet.index(1,0)),QItemSelectionModel::ClearAndSelect);
 window.recalculateNow();require(worksheet.calculatedSnapshot().at("values").size()<10,"manual calculation must retain only derived or exceptional display values, not every literal input");require(worksheet.numericValue(0,0)==42&&worksheet.numericValue(1,0)==84&&worksheet.numericValue(2,0)==.25&&worksheet.numericValue(6,0)==45292,"manual compact output retains numeric literals, formulas, parsed percentages and dates");bool totals=false;for(const auto* label:window.findChildren<QLabel*>())if(label->text().contains("Count: 2")&&label->text().contains("Sum: 126"))totals=true;require(totals,"manual compact output preserves numeric selection status");
 const auto clipboard=worksheet.copyRange(0,0,6,0);require(worksheet.pasteRange(0,2,clipboard,"values"),"manual compact output retains typed Values paste");require(worksheet.raw(0,2).toDouble()==42&&worksheet.raw(1,2).toDouble()==84&&worksheet.raw(2,2).toDouble()==.25&&worksheet.raw(3,2)=="'00123"&&worksheet.raw(4,2)=="TRUE"&&worksheet.raw(5,2)=="#DIV/0!"&&worksheet.raw(6,2).toDouble()==45292,"manual Values paste preserves formula results, percentage/date, quoted numeric text, boolean and error");
}
void cachedSheetSwitchIsImmediate(){
 MainWindow window;auto& worksheet=model(window);Json cells=Json::array();for(int row=1;row<=100000;++row)cells.push_back({{"row",row},{"col",1},{"input",row==100000?"=1+1":"2"}});window.currentSession()=WorkbookSession({{"revision",7},{"sheets",Json::array({{{"id","sheet-1"},{"name","Data"},{"cells",std::move(cells)}}})}});worksheet.setSheetId("sheet-1");require(window.executeCommand("sheet.add"),"tab latency fixture creates second worksheet");window.recalculateNow();auto* tabs=window.findChild<QTabBar*>("workspace.sheets");require(tabs&&tabs->count()==2&&tabs->currentIndex()==1,"tab latency fixture selects second worksheet");QElapsedTimer timer;timer.start();tabs->setCurrentIndex(0);const auto milliseconds=timer.nsecsElapsed()/1e6;std::cout<<"Cached sheet switch ms: "<<milliseconds<<'\n';require(milliseconds<50,"switching to a sheet with current cached results must not synchronously recalculate100000 inputs");require(worksheet.sheetId()=="sheet-1"&&worksheet.numericValue(99999,0)==2,"cached sheet switch must reuse the current destination formula value rather than only raw literal fallback");
 require(worksheet.setData(worksheet.index(0,0),"3",Qt::EditRole),"stale tab fixture changes a source input");timer.restart();tabs->setCurrentIndex(1);const auto staleMilliseconds=timer.nsecsElapsed()/1e6;std::cout<<"Stale sheet switch ms: "<<staleMilliseconds<<'\n';require(staleMilliseconds<50,"a stale sheet switch queues calculation instead of recalculating synchronously");timer.restart();tabs->setCurrentIndex(0);require(timer.nsecsElapsed()/1e6<50&&worksheet.numericValue(0,0)==3,"returning to stale source immediately uses current literal input without an old cache");
}
void formulaViewRetainsNumericStatus(){
 MainWindow window;auto& worksheet=model(window);window.currentSession()=WorkbookSession(Json::parse(R"json({"revision":5,"sheets":[{"id":"s","name":"Data","cells":[{"row":1,"col":1,"input":"2","style":{"numberFormat":"$0.00"}},{"row":1,"col":2,"input":"=A1*3"}]}]})json"));worksheet.setSheetId("s");auto* grid=window.findChild<QTableView*>("worksheet.grid");grid->setCurrentIndex(worksheet.index(0,0));grid->selectionModel()->select(QItemSelection(worksheet.index(0,0),worksheet.index(0,1)),QItemSelectionModel::ClearAndSelect);window.recalculateNow();auto correctStatus=[&](){for(const auto* label:window.findChildren<QLabel*>())if(label->text().contains("Count: 2")&&label->text().contains("Sum: 8"))return true;return false;};require(correctStatus()&&worksheet.data(worksheet.index(0,1)).toString()=="6","formula-view fixture has calculated value and numeric total8");
 require(window.executeCommand("view.formulas"),"Show formulas enables");require(worksheet.data(worksheet.index(0,1)).toString()=="=A1*3"&&worksheet.data(worksheet.index(0,0)).toString()=="$2.00","Show formulas renders raw formulas while retaining numeric literal formatting");require(worksheet.numericValue(0,1)==6&&correctStatus(),"Show formulas must retain typed formula value and numeric selection total8");require(window.executeCommand("view.formulas")&&worksheet.data(worksheet.index(0,1)).toString()=="6"&&correctStatus(),"hiding formulas reuses current calculated value and selection total");
}
void currentFormattingDoesNotQueueCalculation(){
 MainWindow window;auto& worksheet=model(window);
 require(worksheet.setData(worksheet.index(0,0),"42",Qt::EditRole)&&worksheet.setData(worksheet.index(0,1),"=A1*2",Qt::EditRole),"prepare a genuinely calculated formula before formatting");window.recalculateNow();
 auto* grid=window.findChild<QTableView*>("worksheet.grid");grid->selectionModel()->select(QItemSelection(worksheet.index(0,0),worksheet.index(0,1)),QItemSelectionModel::ClearAndSelect);
 const auto totalsCurrent=[&]{for(const auto* label:window.findChildren<QLabel*>())if(label->text().contains("Count: 2")&&label->text().contains("Sum: 126"))return true;return false;};require(totalsCurrent(),"formatting fixture has current selection totals");
 QTimer* calculationDelay=nullptr;for(auto* timer:window.findChildren<QTimer*>())if(timer->isSingleShot()&&timer->interval()==35)calculationDelay=timer;
 require(calculationDelay,"the native automatic calculation queue is available");calculationDelay->stop();
 const auto before=window.currentSession().snapshot();
 require(worksheet.apply(Json::array({{{"type","set_style"},{"sheet",worksheet.sheetId().toStdString()},{"row",1},{"col",2},{"style",{{"fontSize",18}}}}})),"format a calculated cell without changing its input");
 require(!calculationDelay->isActive(),"formatting current results must not queue another whole-workbook calculation");
 require(worksheet.numericValue(0,1)==84&&worksheet.data(worksheet.index(0,1),Qt::FontRole).value<QFont>().pointSizeF()==18,"formatting immediately retains the typed formula result and paints its new font");
 require(totalsCurrent(),"formatting current results keeps selection totals without waiting for recalculation");
 require(window.executeCommand("undo")&&!calculationDelay->isActive()&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"format Undo restores the original cells without scheduling calculation");
 require(window.executeCommand("redo")&&!calculationDelay->isActive()&&worksheet.numericValue(0,1)==84,"format Redo also reuses current formula results");
 require(window.executeCommand("columns.size",{{"size",22.0}})&&!calculationDelay->isActive()&&worksheet.numericValue(0,1)==84,"column sizing also reuses current results without queuing calculation");
 require(window.executeCommand("undo")&&!calculationDelay->isActive()&&worksheet.numericValue(0,1)==84,"column size Undo reuses current results");
 require(worksheet.setData(worksheet.index(0,0),"50",Qt::EditRole)&&calculationDelay->isActive()&&!worksheet.numericValue(0,1).has_value(),"a real input change still invalidates results and queues calculation");
 calculationDelay->stop();
 require(worksheet.apply(Json::array({{{"type","set_style"},{"sheet",worksheet.sheetId().toStdString()},{"row",1},{"col",2},{"style",{{"bold",true}}}}}))&&calculationDelay->isActive()&&!worksheet.numericValue(0,1).has_value(),"formatting stale inputs still schedules the calculation they need");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):"all";if(selected=="all"||selected=="manual")compactManualCalculation();if(selected=="all"||selected=="tabs")cachedSheetSwitchIsImmediate();if(selected=="all"||selected=="formulas")formulaViewRetainsNumericStatus();if(selected=="all"||selected=="formatting")currentFormattingDoesNotQueueCalculation();std::cout<<"manual calculation tests passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
