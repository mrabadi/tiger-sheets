#include "tigersheets/codex.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
bool until(const std::function<bool()>& condition){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<5000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);return condition();}
QString peer(QTemporaryDir& dir){const auto path=dir.filePath("codex-async");QFile file(path);require(file.open(QIODevice::WriteOnly),"write async protocol peer");file.write(R"PY(#!/usr/bin/python3
import json,sys
turn=0
log=open(__file__+'.log','a',buffering=1)
def emit(value): print(json.dumps(value),flush=True)
for line in sys.stdin:
 m=json.loads(line);log.write(json.dumps(m)+'\n');p=m.get('params',{});method=m.get('method')
 def reply(value): emit({'id':m['id'],'result':value})
 if method=='initialize': reply({})
 elif method=='initialized': pass
 elif method=='config/read': reply({'config':{'mcp_servers':{}}})
 elif method=='account/read': reply({'account':{'type':'chatgpt'},'requiresOpenaiAuth':True})
 elif method=='model/list': reply({'data':[{'id':'fixture','model':'fixture','isDefault':True,'supportedReasoningEfforts':['low'],'serviceTiers':['default']}],'nextCursor':None})
 elif method=='thread/start': reply({'thread':{'id':'thread'}})
 elif method=='turn/start':
  turn+=1;reply({'turn':{'id':'turn'+str(turn)}})
  emit({'id':'tool'+str(turn),'method':'item/tool/call','params':{'threadId':'thread','turnId':'turn'+str(turn),'tool':'workbook_v1_read','arguments':{}}})
 elif method=='turn/interrupt':
  reply({});emit({'method':'turn/completed','params':{'threadId':'thread','turn':{'id':'turn'+str(turn),'status':'interrupted'}}})
 elif method is None and 'result' in m:
  if m['result'].get('success'): emit({'method':'turn/completed','params':{'threadId':'thread','turn':{'id':'turn'+str(turn),'status':'completed'}}})
 else: reply({})
)PY");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;}
void asynchronousTools(){QTemporaryDir dir;CodexClient client;std::function<void(Json)> complete;int calls=0,cancelled=0;client.onToolAsync=[&](QString,Json,std::function<void(Json)> callback){++calls;complete=std::move(callback);};client.onCancelTools=[&]{++cancelled;};const auto runtime=peer(dir);client.connectRuntime(runtime);require(until([&]{return client.isReady();}),"async client connects");const Json book={{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Fictional"},{"cells",Json::array()}}})}};
 client.send("read fictional data",book);require(until([&]{return calls==1;}),"async host receives tool call");require(client.isBusy(),"turn remains active while a host tool is pending");complete({{"value",42}});require(until([&]{return !client.isBusy();}),"asynchronous completion replies successfully");
 client.send("read again",book);require(until([&]{return calls==2;}),"second async host call starts");auto late=complete;client.cancel();require(until([&]{return !client.isBusy();}),"Stop interrupts the pending tool turn");late({{"late",true}});QCoreApplication::processEvents();require(cancelled==1,"Stop invokes host job cancellation exactly once");
 QFile log(runtime+".log");require(log.open(QIODevice::ReadOnly),"read async protocol evidence");int successes=0,failures=0;for(const auto& line:log.readAll().split('\n'))if(!line.isEmpty()){const auto message=Json::parse(line.toStdString());if(message.value("id",Json())=="tool2"&&message.contains("result")){if(message["result"].value("success",false))++successes;else ++failures;}}require(successes==0&&failures==1,"a cancelled async tool gets one failed reply and no late success");
 client.send("third read",book);require(until([&]{return calls==3;}),"third async call starts");late=complete;client.resetWorkbook();late({{"foreign",true}});QCoreApplication::processEvents();require(!client.isReady()&&!client.isBusy()&&cancelled==2,"workbook reset cancels the old host job and invalidates its completion");
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{asynchronousTools();std::cout<<"Codex asynchronous tools passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
