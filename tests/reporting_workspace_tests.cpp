#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/chart.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QTableView>
#include <QTemporaryDir>
#include <QDockWidget>
#include <QStatusBar>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());MainWindow window;auto& session=window.currentSession();
 Json operations=Json::array({{{"type","add_sheet"},{"id","report"},{"name","Report"}}});const std::vector<std::vector<std::string>> rows={{"Region","Revenue"},{"East","12"},{"West","18"}};for(int r=0;r<3;++r)for(int c=0;c<2;++c)operations.push_back({{"type","set_cell"},{"sheet","sheet-1"},{"row",r+1},{"col",c+1},{"input",rows[r][c]}});if(!session.apply(0,operations))return 1;
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());model->refresh();
 Json definition={{"id","p1"},{"name","RevenueReport"},{"source",{{"sheet","sheet-1"},{"row1",1},{"col1",1},{"row2",3},{"col2",2}}},{"rows",Json::array({{{"field","Region"}}})},{"values",Json::array({{{"field","Revenue"},{"aggregate","sum"},{"name","Revenue"}}})},{"output",{{"sheet","report"},{"row1",1},{"col1",1}}}};
 if(!window.executeCommand("pivot.create",{{"definition",definition}})){std::cerr<<"desktop starts a native reporting job: "<<window.lastError().toStdString()<<'\n';return 1;}
 QElapsedTimer timer;timer.start();while(!session.snapshotView().contains("pivots")&&timer.elapsed()<10000)app.processEvents();
 if(!session.snapshotView().contains("pivots")||session.snapshotView()["pivots"][0]["version"]!=1){std::cerr<<"native worker commits table, cache and results: "<<window.lastError().toStdString()<<'\n';return 1;}
 if(!window.executeCommand("pivot.chart",{{"id","pc1"},{"pivotId","p1"},{"type","line"}})||session.snapshotView()["charts"][0]["pivotId"]!="p1"){std::cerr<<"desktop inserts a linked native PivotChart\n";return 1;}
 auto* view=dynamic_cast<WorksheetView*>(window.findChild<QTableView*>("worksheet.grid"));const auto point=model->index(1,1);
 if(!view->pointedReference||!view->pointedReference(point,point).contains("GETPIVOTDATA")){std::cerr<<"formula pointing into a report defaults to GETPIVOTDATA\n";return 1;}
 if(!window.executeCommand("pivot.generateGetPivotData",{{"enabled",false}})||!view->pointedReference(point,point).isEmpty()){std::cerr<<"the user can disable automatic GETPIVOTDATA references\n";return 1;}
 if(!window.executeCommand("pivot.fields",{{"id","p1"}})||!window.findChild<QDockWidget*>("workspace.pivotFields")){std::cerr<<"clickable report fields pane is available\n";return 1;}
 auto* fieldsDock=window.findChild<QDockWidget*>("workspace.pivotFields");fieldsDock->hide();window.recalculateNow();ChartWidget* chart=nullptr;for(auto* widget:window.findChildren<QWidget*>())if(auto* candidate=dynamic_cast<ChartWidget*>(widget))chart=candidate;if(!chart)return 1;chart->setSelected(true);if(fieldsDock->isHidden()){std::cerr<<"selecting a PivotChart exposes its owning report fields and filters\n";return 1;}
 if(!window.executeCommand("pivot.delete",{{"id","p1"},{"accept",true}})||!session.snapshotView()["pivots"].empty()||!session.snapshotView()["charts"].empty()){std::cerr<<"pivot deletion removes its linked charts atomically\n";return 1;}
 if(!window.executeCommand("undo")||session.snapshotView()["pivots"].size()!=1||session.snapshotView()["charts"].size()!=1){std::cerr<<"one undo restores the deleted report and chart\n";return 1;}
 if(!window.executeCommand("pivot.refresh",{{"id","p1"}})||!window.property("reportBusy").toBool())return 1;
 if(!window.saveWorkbook(directory.filePath("relocated.xlsx"))||window.property("reportBusy").toBool()){std::cerr<<"Save As must invalidate report jobs bound to the previous workbook location\n";return 1;}
 if(!window.executeCommand("pivot.refresh",{{"id","p1"}})||!window.property("reportBusy").toBool())return 1;
 if(!window.executeCommand("links.refresh")||window.property("reportBusy").toBool()){std::cerr<<"refreshing calculation context cancels an in-flight report immediately\n";return 1;}
 const auto beforeCancel=session.snapshot();
 if(!window.executeCommand("pivot.refresh",{{"id","p1"}})||!window.property("reportBusy").toBool())return 1;
 if(!window.executeCommand("pivot.cancel")||window.property("reportBusy").toBool()||window.statusBar()->currentMessage()!="Report stopped; workbook changes were discarded"||session.snapshot()!=beforeCancel){std::cerr<<"explicit active report cancellation retains its feedback without committing a candidate\n";return 1;}
 timer.restart();while(timer.elapsed()<150)app.processEvents();
 if(session.snapshot()!=beforeCancel){std::cerr<<"a cancelled worker cannot publish a late workbook change\n";return 1;}
 window.statusBar()->clearMessage();
 if(!window.executeCommand("pivot.cancel")||!window.statusBar()->currentMessage().isEmpty()||session.snapshot()!=beforeCancel){std::cerr<<"allocated but idle reporting resources do not announce another discarded report\n";return 1;}
 std::cout<<"reporting workspace tests passed\n";
}
