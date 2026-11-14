#include "tigersheets/main_window.hpp"
#include "tigersheets/codex.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
bool until(const std::function<bool()>& condition,int timeout){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<timeout)QApplication::processEvents(QEventLoop::AllEvents,10);return condition();}
Json withoutRevision(Json value){value.erase("revision");return value;}
void validate(const Json& book){
 require(book.at("sheets").size()==2&&(!book.contains("links")||book.at("links").empty())&&!book.contains("vba"),"fictional live fixture must contain only its two local worksheets");
 require(book.contains("pivots")&&book.at("pivots").size()==1,"live model must create one genuine native report");const auto& pivot=book.at("pivots")[0];require(pivot.at("version")==1&&pivot.at("id")=="live-finance-report"&&pivot.at("layout").at("grandTotals")[0].at("value")==210,"native report must aggregate the six fictional revenues to 210");
 require(book.contains("charts")&&book.at("charts").size()==1&&book.at("charts")[0].value("pivotId",std::string())=="live-finance-report","live model must create a genuine linked PivotChart");
 require(pivot.at("layout").at("rowTotals").size()==2&&pivot.at("layout").at("rowTotals")[0][0].at("value")==90&&pivot.at("layout").at("rowTotals")[1][0].at("value")==120,"regional report totals must be East 90 and West 120");
}
}
int main(int argc,char**argv){
 if(argc<2||std::string(argv[1])!="--live"){std::cout<<"Explicit opt-in required: live_reporting_workflow --live [--runtime /path/to/codex] [--timeout-ms 180000]\n";return 0;}
 QString runtime="codex";int timeout=180000;for(int i=2;i<argc;++i){const std::string option=argv[i];if(option=="--runtime"&&i+1<argc)runtime=QString::fromLocal8Bit(argv[++i]);else if(option=="--timeout-ms"&&i+1<argc){try{timeout=std::stoi(argv[++i]);}catch(...){return 2;}if(timeout<1000||timeout>300000)return 2;}else return 2;}
 QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());QTemporaryDir artifacts(QDir::tempPath()+"/tigersheets-live-reporting-XXXXXX");
 try{
  MainWindow window;if(!qEnvironmentVariableIsEmpty("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"))window.setProperty("reportWorkerExecutable",qEnvironmentVariable("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"));window.resize(1440,1000);window.show();auto* dock=window.findChild<QDockWidget*>("workspace.codex");require(dock,"real desktop Codex dock exists");dock->show();CodexClient* client=nullptr;for(auto* object:window.findChildren<QObject*>())if(auto* candidate=dynamic_cast<CodexClient*>(object))client=candidate;require(client,"real desktop Codex client exists");bool runtimeError=false;const auto displayError=client->onError;client->onError=[&](QString message){runtimeError=true;if(displayError)displayError(std::move(message));};
  std::cout<<"Connecting installed authenticated runtime; only a new fictional workbook will be shared.\n"<<std::flush;client->connectRuntime(runtime);if(!until([&]{return client->isReady()||runtimeError;},30000)||!client->isReady()){std::cerr<<"UNVERIFIED: runtime authentication, policy or network connection unavailable.\n";return 77;}
  auto* composer=dock->findChild<QPlainTextEdit*>("codex.input");auto* send=dock->findChild<QPushButton*>("codex.send");require(composer&&send&&send->isEnabled(),"connected desktop composer is usable");const auto initial=window.currentSession().snapshot();
  composer->setPlainText("Create a small fictional regional revenue report in this empty workbook using only the native workbook tools. Do not use shell, files, browsing, external links or macros. Discover capabilities and read stable worksheet ids first. Rename the first sheet Fictional Sales and put this exact table at A1:D7: headers Region, Quarter, Revenue, Cost; rows East,Q1,10,4; West,Q1,20,8; East,Q2,30,12; West,Q2,40,16; East,Q3,50,20; West,Q3,60,24. Add exactly one worksheet with id live-report-sheet named Reporting. Create a real native PivotTable with id live-finance-report, name FictionalRevenue, source A1:D7 on Fictional Sales, row field Region, column field Quarter, one SUM Revenue value named Revenue with format 0.00, output Reporting!A1. Create one real native column PivotChart linked to that report, id live-finance-chart. Read the proposed report and verify grand total 210, East 90, West 120. Keep all edits in one cumulative preview. Do not accept or save. Say clearly that the values are fictional and the proposal is ready for review.");
  send->click();require(client->isBusy(),"desktop Send starts a real runtime turn");if(!until([&]{return !client->isBusy()||runtimeError;},timeout)||runtimeError){client->cancel();std::cerr<<"UNVERIFIED: authenticated inference did not finish within the runtime/network limits.\n";return 77;}
  require(window.currentSession().hasPreview()&&window.currentSession().snapshot()==initial,"live authored edits remain isolated before desktop acceptance");auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=grid?dynamic_cast<WorksheetModel*>(grid->model()):nullptr;require(model,"real native worksheet model exists");validate(model->workbookView());
  require(window.executeCommand("preview.accept"),"accept through the real desktop command");validate(window.currentSession().snapshotView());require(!window.currentSession().hasPreview(),"desktop acceptance ends the proposal");require(window.executeCommand("undo")&&withoutRevision(window.currentSession().snapshot())==withoutRevision(initial)&&!window.currentSession().canUndo(),"one desktop Undo removes the complete live report proposal");require(window.executeCommand("redo"),"desktop Redo restores the report proposal");validate(window.currentSession().snapshotView());
  const auto path=artifacts.filePath("fictional-native-report.xlsx");require(window.saveWorkbook(path),"save fictional report through desktop persistence");const auto reopened=readWorkbook(path.toStdString());validate(reopened.workbook);bool nativeTable=false,nativeCache=false,nativeChart=false;for(const auto& [part,xml]:reopened.entries){nativeTable|=part.starts_with("xl/pivotTables/")&&part.ends_with(".xml");nativeCache|=part.starts_with("xl/pivotCache/")&&part.ends_with(".xml");nativeChart|=part.starts_with("xl/charts/")&&xml.find("pivotSource")!=std::string::npos;}require(nativeTable&&nativeCache&&nativeChart,"saved package contains real native table, cache and PivotChart parts");artifacts.setAutoRemove(false);std::cout<<"PASS: authenticated fictional report authored, reviewed, accepted, undone/redone and saved/reopened.\nFixture: "<<path.toStdString()<<"\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
