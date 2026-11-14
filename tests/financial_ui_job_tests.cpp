#include "tigersheets/main_window.hpp"
#include "tigersheets/financial_model_pane.hpp"
#include "tigersheets/grid.hpp"
#include <QTableView>
#include <QLabel>
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);qputenv("TIGERSHEETS_STATE_DIR","/tmp/tigersheets-financial-ui-tests");QTemporaryDir dir;try{
 QFile fake(dir.filePath("financial-worker"));require(fake.open(QIODevice::WriteOnly),"create deterministic private financial worker");fake.write(R"py(#!/usr/bin/python3
import json,sys
request=json.load(open(sys.argv[sys.argv.index('--request')+1]))
result=dict(version=1,kind='financial_model',status='ok',revision=request['workbook']['revision'],contextGeneration=request['contextGeneration'],workbookId=request['workbookId'],diagnostics=[],analysis=dict(success=True,status='saved'),operations=[dict(type='save_financial_definition',definition=request['definition'])])
json.dump(result,open(sys.argv[sys.argv.index('--response')+1],'w'))
)py");fake.close();fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 MainWindow window;window.setProperty("reportWorkerExecutable",fake.fileName());auto& session=window.currentSession();const Json definition={{"version",1},{"id","base"},{"kind","assumption_set"},{"name","Base"},{"cells",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",1},{"type","number"},{"value",10}}})}};
 require(window.executeCommand("financial.save",{{"definition",definition}}),"desktop financial Save starts isolated review job");QElapsedTimer timer;timer.start();while(window.property("financialBusy").toBool()&&timer.elapsed()<10000){app.processEvents();QThread::msleep(1);}require(window.lastError().isEmpty(),window.lastError().toUtf8().constData());require(session.hasPreview(),"desktop financial Save publishes a reviewable preview");require(!session.snapshotView().contains("financialModels"),"live workbook remains unchanged before acceptance");require(window.executeCommand("preview.accept"),"desktop acceptance applies one financial transaction");require(session.snapshotView().at("financialModels")[0].at("id")=="base","accepted typed model configuration is stored");require(session.undo()&&!session.snapshotView().contains("financialModels"),"one undo removes accepted saved configuration");FinancialModelPane* pane=nullptr;for(auto* widget:window.findChildren<QWidget*>())if(auto* candidate=dynamic_cast<FinancialModelPane*>(widget))pane=candidate;require(pane&&bool(pane->saveRequested),"financial pane Save is connected to isolated desktop workflow");pane->saveRequested(definition);timer.restart();while(window.property("financialBusy").toBool()&&timer.elapsed()<10000){app.processEvents();QThread::msleep(1);}require(session.hasPreview(),"financial pane Save exposes the same review and acceptance flow");require(window.executeCommand("preview.discard"),"pane-generated proposal can be discarded");std::string error;const bool extra=session.apply(session.snapshotView().at("revision"),Json::array({{{"type","add_sheet"},{"id","other"},{"name","Other"}}}),&error);require(extra,error.c_str());require(window.executeCommand("financial.save",{{"definition",definition}}),"first financial job starts");require(!window.executeCommand("financial.example"),"competing financial example refuses while first job captures inputs");timer.restart();while(!session.hasPreview()&&timer.elapsed()<10000){app.processEvents();QThread::msleep(1);}auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(session.hasPreview()&&model&&model->sheetId()=="sheet-1","rejected competing commands cannot redirect the original job's preview worksheet");auto* summary=window.findChild<QLabel*>("financial.status");require(summary&&summary->text().contains("saved",Qt::CaseInsensitive),"qualified preview retains its financial analysis status for review");require(window.executeCommand("preview.discard"),"discard concurrent publication fixture");std::cout<<"financial UI job tests passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
