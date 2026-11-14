#include "tigersheets/financial_workspace.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;try{
 QFile fake(dir.filePath("financial-worker"));require(fake.open(QIODevice::WriteOnly),"create deterministic private financial worker");fake.write(R"py(#!/usr/bin/python3
import json,sys,time
request=json.load(open(sys.argv[sys.argv.index('--request')+1]))
print(json.dumps(dict(version=1,type='progress',stage='Financial trial entered')),flush=True)
if request['definition']['name']=='Delayed': time.sleep(0.4)
if request['definition']['name']=='Crash': sys.exit(17)
result=dict(version=1,kind='financial_model',status='ok',revision=request['workbook']['revision'],contextGeneration=request['contextGeneration'],workbookId=request['workbookId'],diagnostics=[],analysis=dict(success=True,status='saved'),operations=[dict(type='save_financial_definition',definition=request['definition'])])
json.dump(result,open(sys.argv[sys.argv.index('--response')+1],'w'))
)py");fake.close();fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 WorkbookSession session;FinancialWorkspace host(session);host.executable=fake.fileName();std::uint64_t generation=3;host.epoch=[&]{return std::pair<std::string,std::uint64_t>{"book-a",generation};};Json retained=Json::array();host.retainedLinks=[&]() ->const Json&{return retained;};QString failure;int previews=0;host.onError=[&](QString value){failure=value;};host.onPreview=[&]{++previews;};const Json definition={{"version",1},{"id","base"},{"kind","assumption_set"},{"name","Base"},{"cells",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",1},{"type","number"},{"value",10}}})}};
 require(host.start(definition,"save"),"saved assumption starts isolated capture and worker");QElapsedTimer timer;timer.start();while(host.running()&&timer.elapsed()<10000){app.processEvents();QThread::msleep(1);}require(failure.isEmpty(),failure.toUtf8().constData());require(previews==1&&session.hasPreview(),"successful captured job publishes a reviewable prepared preview");require(!session.snapshotView().contains("financialModels"),"live workbook remains unchanged before acceptance");std::string error;std::unique_ptr<WorkbookSession::PreparedTransaction> retired;require(session.acceptPrepared(retired,&error),"one acceptance publishes saved setup");require(session.snapshotView().at("financialModels")[0].at("id")=="base","accepted typed model configuration is stored");require(session.undo()&&!session.snapshotView().contains("financialModels"),"one undo removes accepted saved configuration");bool entered=false;host.onProgress=[&](QString stage){if(stage=="Financial trial entered")entered=true;};auto delayed=definition;delayed["name"]="Delayed";const auto waitFor=[&](const auto& condition){QElapsedTimer deadline;deadline.start();while(!condition()&&deadline.elapsed()<10000){app.processEvents();QThread::msleep(1);}require(condition(),"isolated financial worker finishes within timeout");};failure.clear();require(host.start(delayed,"save"),"delayed financial worker starts");waitFor([&]{return entered;});host.cancel();waitFor([&]{return !host.running();});require(!session.hasPreview()&&previews==1,"stopping an active financial worker discards all unpublished changes");entered=false;require(host.start(delayed,"save"),"new financial job starts after cancelled worker shutdown");waitFor([&]{return entered;});++generation;waitFor([&]{return !host.running();});require(!session.hasPreview()&&failure.contains("changed")&&previews==1,"changed calculation generation refuses asynchronous publication");failure.clear();auto crash=definition;crash["name"]="Crash";require(host.start(crash,"save"),"crash fixture starts isolated worker");waitFor([&]{return !host.running();});require(!failure.isEmpty()&&!session.hasPreview()&&previews==1,"worker failure cannot publish a financial candidate");std::cout<<"financial workspace job tests passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
