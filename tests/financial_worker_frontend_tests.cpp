#include "tigersheets/report_worker.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
 QTemporaryDir temporary;QFile child(temporary.filePath("financial-worker"));require(child.open(QIODevice::WriteOnly),"create fake process boundary");child.write(R"python(#!/usr/bin/python3
import json,os,sys
args=dict(zip(sys.argv[1::2],sys.argv[2::2]))
request=json.load(open(args['--request']))
result={'version':1,'kind':'financial_model','status':'ok','revision':request['workbook']['revision'],'contextGeneration':request['contextGeneration'],'workbookId':request['workbookId'],'diagnostics':[],'operations':[{'type':'set_cell','sheet':'s','row':1,'col':1,'input':'12'}],'analysis':{'status':'converged','success':True,'result':24}}
if request['definition']['name']=='Unqualified':result['analysis']={'status':'limit','success':False}
with open(args['--response'],'w') as output:json.dump(result,output)
os.chmod(args['--response'],0o600)
)python");child.close();require(child.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"private executable fake process");
 const Json request={{"version",1},{"kind","financial_model"},{"action","run"},{"workbookId","financial-book"},{"contextGeneration",3},{"retainedLinks",Json::array()},{"workbook",{{"revision",7},{"sheets",Json::array({{{"id","s"},{"name","Model"},{"cells",Json::array()}}})}}},{"definition",{{"version",1},{"id","seek-profit"},{"name","Profit target"},{"kind","goal_seek"},{"specification",Json::object()}}}};
 ReportWorker worker;Json result;worker.onResult=[&](Json completed){result=std::move(completed);};require(worker.start(std::make_shared<const Json>(request),child.fileName()),"financial model starts through isolated worker frontend");QElapsedTimer elapsed;elapsed.start();while(worker.running()&&elapsed.elapsed()<5000)app.processEvents(QEventLoop::AllEvents,10);
 require(!worker.running(),"financial result completes");require(worker.error().isEmpty(),qPrintable(worker.error()));require(result.contains("analysis")&&result.at("analysis").at("success")==true,"financial worker must deliver qualified analysis and bounded preview operations");require(result.at("operations").size()==1&&result.at("revision")==7&&result.at("contextGeneration")==3,"financial result retains exact workbook epoch and operations");auto invalid=request;invalid["definition"]["name"]="Unqualified";ReportWorker denied;int published=0;denied.onResult=[&](Json){++published;};require(denied.start(std::make_shared<const Json>(invalid),child.fileName()),"unqualified financial boundary job starts");elapsed.restart();while(denied.running()&&elapsed.elapsed()<5000)app.processEvents(QEventLoop::AllEvents,10);require(!denied.running()&&published==0&&!denied.error().isEmpty(),"nonconverged financial results must not publish mutating operations");std::cout<<"financial worker frontend tests passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
