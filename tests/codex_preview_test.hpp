#pragma once
#include "tigersheets/main_window.hpp"
#include "tigersheets/codex.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <stdexcept>

namespace tigersheets::test {
// Exercise the real desktop asynchronous host and native isolated child. Only
// the authenticated model boundary is replaced by a deterministic local peer.
inline Json isolatedPreview(MainWindow& window,const Json& arguments){
 CodexClient* client=nullptr;for(auto* object:window.findChildren<QObject*>())if(auto* candidate=dynamic_cast<CodexClient*>(object))client=candidate;
 if(!client||!client->onToolAsync)throw std::runtime_error("desktop asynchronous workbook host is unavailable");
 if(!qEnvironmentVariableIsEmpty("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"))window.setProperty("reportWorkerExecutable",qEnvironmentVariable("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"));
 QTemporaryDir directory;const auto path=directory.filePath("codex-preview");QFile request(path+".request");if(!request.open(QIODevice::WriteOnly))throw std::runtime_error("cannot write fictional preview fixture");request.write(QByteArray::fromStdString(arguments.dump()));request.close();
 QFile script(path);if(!script.open(QIODevice::WriteOnly))throw std::runtime_error("cannot write deterministic preview peer");script.write(R"PY(#!/usr/bin/python3
import json,sys
with open(__file__+'.request') as f: arguments=json.load(f)
def emit(v): print(json.dumps(v),flush=True)
for line in sys.stdin:
 m=json.loads(line);method=m.get('method')
 if method=='initialized': continue
 if method is None:
  if m.get('id')=='preview-tool':
   with open(__file__+'.reply','w') as f:json.dump(m['result'],f)
   emit({'method':'turn/completed','params':{'threadId':'preview-thread','turn':{'id':'preview-turn','status':'completed'}}})
  continue
 result={}
 if method=='initialize':result={'userAgent':'desktop-preview-regression','platformOs':'linux'}
 elif method=='config/read':result={'config':{'mcp_servers':{}}}
 elif method=='account/read':result={'account':{'type':'chatgpt'},'requiresOpenaiAuth':True}
 elif method=='model/list':result={'data':[{'id':'fixture','model':'fixture','isDefault':True,'supportedReasoningEfforts':['low']}],'nextCursor':None}
 elif method=='thread/start':result={'thread':{'id':'preview-thread'}}
 elif method=='turn/start':result={'turn':{'id':'preview-turn','status':'inProgress'}}
 emit({'id':m['id'],'result':result})
 if method=='turn/start':emit({'id':'preview-tool','method':'item/tool/call','params':{'threadId':'preview-thread','turnId':'preview-turn','tool':'workbook_v1_preview','arguments':arguments}})
)PY");script.close();if(!script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))throw std::runtime_error("cannot execute deterministic preview peer");
 QString error;struct Restore {CodexClient* client;std::function<void(QString)> previous;~Restore(){client->onError=std::move(previous);}} restore{client,client->onError};client->onError=[&](QString value){error=std::move(value);};
 auto until=[&](const auto& condition){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<30000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);return condition();};
 client->connectRuntime(path);if(!until([&]{return client->isReady()||!error.isEmpty();})||!client->isReady())throw std::runtime_error("deterministic preview runtime failed to connect: "+error.toStdString());
 client->send("Prepare the native desktop regression preview",window.currentSession().snapshotView());if(!until([&]{return !client->isBusy()||!error.isEmpty();})||!error.isEmpty())throw std::runtime_error("desktop isolated preview failed: "+error.toStdString());
 QFile response(path+".reply");if(!response.open(QIODevice::ReadOnly))throw std::runtime_error("desktop did not reply to the isolated preview tool");const auto result=Json::parse(response.readAll().toStdString());return Json::parse(result.at("contentItems")[0].at("text").get<std::string>());
}
}
