#include "tigersheets/main_window.hpp"
#include "tigersheets/snapshot_capture.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <iostream>
using namespace tigersheets;
namespace {
bool waitReport(MainWindow& window){QEventLoop loop;QTimer timer;QElapsedTimer elapsed;elapsed.start();QObject::connect(&timer,&QTimer::timeout,[&]{if(!window.property("reportBusy").toBool()||elapsed.elapsed()>30000)loop.quit();});timer.start(1);if(window.property("reportBusy").toBool())loop.exec();return !window.property("reportBusy").toBool();}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());QFile file(directory.filePath("formulas.csv"));if(!file.open(QIODevice::WriteOnly))return 2;file.write("Region,Revenue\n");for(int row=0;row<5000;++row)file.write("A,=1+1\n");file.close();MainWindow window;if(argc>1)window.setProperty("reportWorkerExecutable",QString::fromLocal8Bit(argv[1]));if(!window.openWorkbook(file.fileName()))return 2;
 Json definition={{"id","p"},{"name","FormulaReport"},{"source",{{"sheet","sheet-1"},{"row1",1},{"col1",1},{"row2",5001},{"col2",2}}},{"rows",Json::array({{{"field","Region"}}})},{"values",Json::array({{{"field","Revenue"},{"aggregate","sum"}}})},{"output",{{"sheet",""},{"row1",1},{"col1",1}}}};
 if(!window.executeCommand("pivot.create",{{"definition",definition}})||!waitReport(window))return 2;window.recalculateNow();const auto revision=window.currentSession().snapshotView().at("revision");const auto history=window.currentSession().historyBytes();bool recalculated=false,sliced=false;
 const auto connection=QObject::connect(window.statusBar(),&QStatusBar::messageChanged,[&](const QString& message){if(message!="Preparing current source values…")return;QTimer::singleShot(0,&window,[&]{QTimer::singleShot(0,&window,[&]{for(auto* object:window.findChildren<QObject*>())if(auto* capture=dynamic_cast<SnapshotCapture*>(object);capture&&capture->running())sliced=true;recalculated=true;window.recalculateNow();});});});
 if(!window.executeCommand("pivot.refresh",{{"id","p"}})||!waitReport(window))return 2;QObject::disconnect(connection);
 const bool unchanged=window.currentSession().snapshotView().at("revision")==revision&&window.currentSession().historyBytes()==history;const bool discarded=window.lastError().contains("changed",Qt::CaseInsensitive);std::cout<<Json{{"recalculatedBetweenSnapshotSlices",recalculated&&sliced},{"candidateDiscarded",discarded},{"unchangedRevisionAndHistory",unchanged},{"error",window.lastError().toStdString()}}.dump()<<std::endl;
 if(!recalculated||!sliced||!unchanged||!discarded){std::cerr<<"Replacing a current calculation during a sliced warm snapshot must discard the candidate\n";return 1;}
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());model->setSheetId("sheet-1");if(!model->setData(model->index(1,1),"=3",Qt::EditRole))return 2;window.recalculateNow();const auto lateRevision=window.currentSession().snapshotView().at("revision");const auto lateHistory=window.currentSession().historyBytes();bool replacedAfterCapture=false;
 const auto lateConnection=QObject::connect(window.statusBar(),&QStatusBar::messageChanged,[&](const QString& message){if(message=="Starting report calculation"&&!replacedAfterCapture){replacedAfterCapture=true;window.recalculateNow();}});
 if(!window.executeCommand("pivot.refresh",{{"id","p"}})||!waitReport(window))return 2;QObject::disconnect(lateConnection);const bool lateDiscarded=replacedAfterCapture&&window.currentSession().snapshotView().at("revision")==lateRevision&&window.currentSession().historyBytes()==lateHistory&&window.currentSession().snapshotView().at("pivots")[0].at("layout").at("grandTotals")[0].at("value")==10000;
 std::cout<<Json{{"replacedAfterSnapshotBeforeWorker",replacedAfterCapture},{"lateCandidateDiscarded",lateDiscarded}}.dump()<<std::endl;if(!lateDiscarded){std::cerr<<"Warm candidate must remain bound to its calculation through publication\n";return 1;}return 0;
}
