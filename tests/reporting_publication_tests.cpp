#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/snapshot_capture.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <algorithm>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){
 QApplication app(argc,argv);const int records=argc>1?std::stoi(argv[1]):10000;QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());QFile file(directory.filePath("report.csv"));if(!file.open(QIODevice::WriteOnly))return 2;file.write("Region,Revenue,C,D,E,F,G,H,I,J\n");for(int row=0;row<records;++row)file.write(QByteArray::number(row%10)+(row==1?",=1+1,3,4,5,6,7,8,9,10\n":",2,3,4,5,6,7,8,9,10\n"));file.close();MainWindow window;if(!window.openWorkbook(file.fileName()))return 2;if(argc>2)window.setProperty("reportWorkerExecutable",QString::fromLocal8Bit(argv[2]));window.show();app.processEvents();window.findChild<QTableView*>("worksheet.grid")->viewport()->repaint();
 Json definition={{"id","p"},{"name","RevenueReport"},{"source",{{"sheet","sheet-1"},{"row1",1},{"col1",1},{"row2",records+1},{"col2",10}}},{"rows",Json::array({{{"field","Region"}}})},{"values",Json::array({{{"field","Revenue"},{"aggregate","sum"}}})},{"output",{{"sheet",""},{"row1",1},{"col1",1}}}};
 bool qualified=true;Json phases=Json::array();
 auto measure=[&](const char* name,const QString& command,const Json& args,int budget){
  QElapsedTimer overall,gap;overall.start();gap.start();Json stages=Json::array(),gaps=Json::array();QString stage;double maximum=0;int maxCaptures=0;QEventLoop loop;QTimer monitor;monitor.setTimerType(Qt::PreciseTimer);monitor.setInterval(1);
  const auto status=QObject::connect(window.statusBar(),&QStatusBar::messageChanged,[&](const QString& message){if(message==stage)return;stage=message;stages.push_back({{"elapsedMs",overall.nsecsElapsed()/1e6},{"stage",message.toStdString()}});});
  QObject::connect(&monitor,&QTimer::timeout,[&]{const auto delay=gap.nsecsElapsed()/1e6;gap.restart();maximum=std::max(maximum,delay);int captures=0;for(auto* object:window.findChildren<QObject*>())if(auto* capture=dynamic_cast<SnapshotCapture*>(object);capture&&capture->running())++captures;maxCaptures=std::max(maxCaptures,captures);if(delay>=10)gaps.push_back({{"elapsedMs",overall.nsecsElapsed()/1e6},{"gapMs",delay},{"stage",stage.toStdString()},{"activeCaptures",captures}});if(!window.property("reportBusy").toBool()||overall.elapsed()>=120000)loop.quit();});
  monitor.start();if(!window.executeCommand(command,args)){QObject::disconnect(status);std::cerr<<window.lastError().toStdString();return false;}if(window.property("reportBusy").toBool())loop.exec();monitor.stop();QObject::disconnect(status);QElapsedTimer paint;paint.start();window.findChild<QTableView*>("worksheet.grid")->viewport()->repaint();const auto paintMs=paint.nsecsElapsed()/1e6;maximum=std::max(maximum,gap.nsecsElapsed()/1e6);Json callbackTimes=Json::object();const auto reported=window.property("reportPublicationTimings").toMap();for(auto it=reported.cbegin();it!=reported.cend();++it)callbackTimes[it.key().toStdString()]=it.value().toDouble();const bool passed=maxCaptures<=1&&maximum<100&&(records!=100000||overall.elapsed()<budget);qualified&=passed;Json phase={{"phase",name},{"finalViewportPaintMs",paintMs},{"publicationCallbackCumulativeMs",callbackTimes},{"sourceCells",records*10},{"sourceFormulas",1},{"longestUiDispatchMs",maximum},{"elapsedMs",overall.elapsed()},{"budgetMs",budget},{"maxConcurrentCaptures",maxCaptures},{"stageTransitions",stages},{"microGaps",gaps},{"qualified",passed},{"measurementBoundary","Whole report action on actual Qt event loop; precise1ms observer, no polling sleep"}};phases.push_back(phase);std::cout<<phase.dump()<<std::endl;return window.currentSession().snapshotView().contains("pivots");
 };
 if(!measure("initial","pivot.create",{{"definition",definition}},15000))return 2;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());if(!model)return 2;
 model->setSheetId("sheet-1");if(!model->setData(model->index(2,1),"=1+2",Qt::EditRole))return 2;if(!measure("fresh","pivot.refresh",{{"id","p"}},10000))return 2;
 model->setSheetId("sheet-1");if(!model->setData(model->index(2,1),"=2+2",Qt::EditRole))return 2;QElapsedTimer sharedCalculation;sharedCalculation.start();window.recalculateNow();const auto sharedCalculationMs=sharedCalculation.nsecsElapsed()/1e6;if(!measure("warm-current-calculated","pivot.refresh",{{"id","p"}},3000))return 2;
 const auto total=[&]{return window.currentSession().snapshotView().at("pivots")[0].at("layout").at("grandTotals")[0].at("value");};
 const bool oneUndo=window.executeCommand("undo")&&total()==records*2+1&&window.currentSession().cellAt("sheet-1",3,2)->at("input")=="=2+2"&&window.executeCommand("redo")&&total()==records*2+2;qualified&=oneUndo;
 const auto revision=window.currentSession().snapshotView().at("revision");const auto history=window.currentSession().historyBytes();window.recalculateNow();if(!measure("unchanged-current-calculated","pivot.refresh",{{"id","p"}},3000))return 2;const bool unchanged=window.currentSession().snapshotView().at("revision")==revision&&window.currentSession().historyBytes()==history;qualified&=unchanged;
 const auto& pivot=window.currentSession().snapshotView().at("pivots")[0];const bool totals=pivot.at("layout").at("grandTotals")[0].at("value")==records*2+2;qualified&=totals;std::cout<<Json{{"summary",true},{"records",records},{"fields",10},{"sourceCells",records*10},{"sourceFormulas",1},{"qualified",qualified},{"sharedCalculationBeforeWarmMs",sharedCalculationMs},{"unchangedRevisionAndHistory",unchanged},{"warmReportOneUndoRedo",oneUndo},{"totalsValidated",totals},{"phases",phases}}.dump()<<std::endl;if(!qualified){std::cerr<<"Whole-pipeline report budgets/concurrency/history qualification failed\n";return 1;}return 0;
}
