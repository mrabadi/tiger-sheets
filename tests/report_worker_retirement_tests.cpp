#include "tigersheets/report_worker.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
QString child(QTemporaryDir& directory,bool foreign){const auto path=directory.filePath(foreign?"foreign":"cancel");QFile file(path);require(file.open(QIODevice::WriteOnly),"write deterministic result child");file.write(R"PY(#!/usr/bin/python3
import json,sys,os
args=dict(zip(sys.argv[1::2],sys.argv[2::2]))
with open(args['--request']) as f:request=json.load(f)
with open(__file__+'.directory','w') as f:f.write(os.path.dirname(args['--request']))
result={'version':1,'revision':request['workbook']['revision']+(1 if 'foreign' in __file__ else 0),'contextGeneration':request['contextGeneration'],'status':'ok','definition':{},'diagnostics':[]}
cell=json.dumps({'row':1,'col':1,'input':'Fictional financial source','style':{'bold':True,'fill':'#e7efe6','numberFormat':'$#,##0.00','font':'DejaVu Sans','horizontal':'right'}})
with open(args['--response'],'w') as f:
 f.write(json.dumps(result)[:-1]+',"cells":[')
 for index in range(200000):f.write((',' if index else '')+cell)
 f.write(']}')
)PY");file.close();require(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"private result executable");return path;}
struct CancelParsed final:QObject {ReportWorker& worker;bool reading=false,cancelled=false;explicit CancelParsed(ReportWorker& value):worker(value){}bool eventFilter(QObject*,QEvent* event) override {if(reading&&!cancelled&&event->type()==QEvent::MetaCall){cancelled=true;worker.stop();}return false;}};
void retirement(bool foreign){QTemporaryDir directory;const auto executable=child(directory,foreign);ReportWorker worker;CancelParsed filter(worker);if(!foreign)worker.installEventFilter(&filter);bool rejected=false,published=false,cancelled=false;worker.onError=[&](QString){rejected=true;};worker.onResult=[&](Json){published=true;};worker.onProgress=[&](QString stage){if(stage=="Reading report result")filter.reading=true;if(stage=="Cancelled")cancelled=true;};auto request=std::make_shared<const Json>(Json{{"version",1},{"workbook",{{"revision",0},{"sheets",Json::array()}}},{"definition",Json::object()},{"retainedLinks",Json::array()},{"contextGeneration",17}});require(worker.start(request,executable),"start isolated result retirement fixture");QElapsedTimer total,after;total.start();double longest=0;QString jobDirectory;bool completed=false;
 while(total.elapsed()<60000){QElapsedTimer dispatch;dispatch.start();QCoreApplication::processEvents(QEventLoop::AllEvents,1);longest=std::max(longest,dispatch.nsecsElapsed()/1e6);if(jobDirectory.isEmpty()){QFile marker(executable+".directory");if(marker.open(QIODevice::ReadOnly))jobDirectory=QString::fromUtf8(marker.readAll());}if(!completed&&(foreign?rejected:cancelled)){completed=true;after.start();}if(completed&&after.elapsed()>300&&!jobDirectory.isEmpty()&&!QFileInfo::exists(jobDirectory))break;QThread::msleep(1);}
 std::cout<<(foreign?"Foreign":"Cancelled parsed")<<" result longest GUI dispatch ms: "<<longest<<'\n';require(!published&&(foreign?rejected:(cancelled&&filter.cancelled)),"foreign and cancelled parsed results never publish");require(!jobDirectory.isEmpty()&&!QFileInfo::exists(jobDirectory),"private job files clean up after refused result");require(longest<100,"retiring a large refused result must keep every GUI dispatch below 100 ms");}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{retirement(true);retirement(false);std::cout<<"report worker retirement tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
