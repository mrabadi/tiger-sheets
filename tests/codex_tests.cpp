#include "tigersheets/codex.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool until(const std::function<bool()>& predicate) {
 QElapsedTimer timer;timer.start();
 while(!predicate()&&timer.elapsed()<5000)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
 return predicate();
}
QString fakeRuntime(QTemporaryDir& directory,QString scenario="normal") {
 const auto path=directory.filePath("codex-"+scenario);QFile file(path);require(file.open(QIODevice::WriteOnly),"fake file");
 file.write(R"PY(#!/usr/bin/python3
import json, os, sys
log=open(__file__+'.log','a',buffering=1)
with open(__file__+'.environment','w') as environment: json.dump({'cwd':os.getcwd(),'env':dict(os.environ),'args':sys.argv[1:]},environment)
turn=0
for line in sys.stdin:
 m=json.loads(line);log.write(json.dumps(m)+'\n');p=m.get('params',{});method=m.get('method')
 if method is None:
  if m['id']=='tool-good':
   print(json.dumps({'method':'item/agentMessage/delta','params':{'threadId':'thread-1','turnId':'turn-'+str(turn),'delta':' done'}}),flush=True)
   print(json.dumps({'method':'turn/completed','params':{'threadId':'thread-1','turn':{'id':'turn-'+str(turn),'status':'completed'}}}),flush=True)
  continue
 if method=='initialize': r={'userAgent':'fake','platformOs':'linux'}
 elif method=='initialized': continue
 elif method=='config/read': r={'config':{}} if 'badconfig' in __file__ else {'config':{'mcp_servers':{'external':{'enabled':True}}}}
 elif method=='account/read': r={'account':{'type':'chatgpt'},'requiresOpenaiAuth':True}
 elif method in ('thread/start','thread/resume'): r={'thread':{'id':'thread-1'}}
 elif method=='turn/interrupt':
  print(json.dumps({'id':m['id'],'result':{}}),flush=True)
  if 'late' in __file__:
   print(json.dumps({'id':'tool-late','method':'item/tool/call','params':{'threadId':'thread-1','turnId':'turn-'+str(turn),'tool':'workbook_v1_preview','arguments':{'revision':7,'operations':[]}}}),flush=True)
   print(json.dumps({'method':'item/agentMessage/delta','params':{'threadId':'thread-1','turnId':'turn-'+str(turn),'delta':'late text'}}),flush=True)
  print(json.dumps({'method':'turn/completed','params':{'threadId':'thread-1','turn':{'id':'turn-'+str(turn),'status':'interrupted'}}}),flush=True)
  continue
 elif method=='turn/start':
  turn+=1;r={'turn':{'id':'turn-'+str(turn),'status':'inProgress'}}
  print(json.dumps({'id':m['id'],'result':r}),flush=True)
  if 'cancel' in __file__: continue
  print(json.dumps({'method':'item/agentMessage/delta','params':{'threadId':'thread-1','turnId':'turn-'+str(turn),'delta':'Working'}}),flush=True)
  for request_id,thread_id,namespace in [('tool-foreign','thread-1','foreign'),('tool-wrong','wrong-thread',None),('tool-good','thread-1',None)]:
   print(json.dumps({'id':request_id,'method':'item/tool/call','params':{'threadId':thread_id,'turnId':'turn-'+str(turn),'tool':'workbook_v1_preview','namespace':namespace,'arguments':{'revision':7,'operations':[{'type':'set_cell','sheet':'sheet-1','row':1,'col':1,'input':'42'}]}}}),flush=True)
  continue
 elif method=='model/list':
  if 'empty' in __file__: r={'data':[],'nextCursor':None}
  elif 'settings' in __file__: r={'data':[{'id':'hidden','model':'hidden','hidden':True,'isDefault':True},{'id':'other','model':'other','isDefault':True,'supportedReasoningEfforts':['medium','high'],'serviceTiers':['standard']},{'id':'sol','model':'gpt-6.1-sol','isDefault':False,'supportedReasoningEfforts':[{'reasoningEffort':'high','description':'Careful'},'low','ultra'],'serviceTiers':[{'id':'fast','name':'Fast','description':'Extra credits'}]}],'nextCursor':None}
  elif 'paged' in __file__ and not p.get('cursor'): r={'data':[{'id':'first','model':'first','displayName':'First','isDefault':False}],'nextCursor':'page2'}
  else: r={'data':[{'id':'available-model','model':'available-model','displayName':'Available','isDefault':True}],'nextCursor':None}
 else: r={}
 print(json.dumps({'id':m['id'],'result':r}),flush=True)
)PY");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;
}
Json messages(const QString& script) {QFile file(script+".log");require(file.open(QIODevice::ReadOnly),"protocol log");Json result=Json::array();const auto bytes=file.readAll();for(const auto& line:bytes.left(bytes.lastIndexOf('\n')+1).split('\n'))if(!line.isEmpty())result.push_back(Json::parse(line.toStdString()));return result;}
void handshake() {
 QTemporaryDir dir;const auto script=fakeRuntime(dir);CodexClient client;QString error;
 client.onError=[&](QString message){error=message;};
 client.connectRuntime(script);
 require(until([&]{return client.isReady()||!error.isEmpty();}),"runtime did not finish connecting");
 require(error.isEmpty(),qPrintable(error));require(client.isReady(),"initialized runtime must be ready");
 require(client.models().size()==1&&client.model()=="available-model","model picker uses discovered default");
 const auto log=messages(script);
 require(log[0].at("method")=="initialize","initialize first");
 require(log[0].at("params").at("capabilities").at("experimentalApi")==true,"dynamic tools require opt in");
 require(log[1].at("method")=="initialized","handshake acknowledgement");
 require(log[2].at("method")=="config/read","effective MCP config checked before auth or thread creation");
 QFile environment(script+".environment");require(environment.open(QIODevice::ReadOnly),"launch information");const auto launch=Json::parse(environment.readAll().toStdString());
 require(launch.at("args")==Json::array({"app-server","--listen","stdio://"}),"documented stdio transport");
 require(QString::fromStdString(launch.at("cwd").get<std::string>()).contains("tiger-sheets-codex-"),"runtime cwd is private");
 require(!launch.at("env").contains("TIGERSHEETS_TEST_PRIVATE_SECRET"),"arbitrary environment secrets are not inherited");
}
void conversation() {
 QTemporaryDir dir;const auto script=fakeRuntime(dir);CodexClient client;QString error,output;int tools=0;
 client.onError=[&](QString message){error=message;};client.onText=[&](QString delta){output+=delta;};
 client.onTool=[&](const QString& name,const Json& arguments){++tools;require(name=="workbook_v1_preview","known tool routed");require(arguments.at("revision")==7,"tool revision preserved");return Json{{"preview",true}};};
 client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"ready timeout");
 const Json book={{"revision",7},{"sheets",Json::array({{{"id","sheet-1"},{"name","Sheet1"},{"cells",Json::array()}}})}};
 for(int attempt=0;attempt<2;++attempt){
  client.send("Set A1",book);require(client.isBusy(),"sending must begin a turn");
  require(until([&]{return !client.isBusy()||!error.isEmpty();}),"turn timeout");require(error.isEmpty(),qPrintable(error));
 }
 require(output=="Working doneWorking done","streamed text across two turns");require(tools==2,"foreign namespace and wrong-thread calls must not reach host");
 const auto log=messages(script);int starts=0,turns=0,failedTools=0;
 for(const auto& m:log){
  if(m.value("method",std::string())=="thread/start"){
   ++starts;const auto& p=m.at("params");require(p.at("sandbox")=="read-only"&&p.at("approvalPolicy")=="never","restricted workbook conversation");
   require(p.at("config").at("mcp_servers").at("external").at("enabled")==false,"inherited MCP server must be disabled");
   require(p.at("dynamicTools")==CodexClient::toolDefinitions(),"the complete native workbook and reporting tool contract is registered");
  }
  if(m.value("method",std::string())=="turn/start"){++turns;require(m.at("params").at("threadId")=="thread-1","same conversation continued");}
  if(m.contains("result")&&m.at("result").contains("success")&&!m.at("result").at("success").get<bool>())++failedTools;
 }
 require(starts==1&&turns==2,"one thread serves repeated messages");require(failedTools==4,"invalid tool calls receive failed replies");
 client.resetWorkbook();require(!client.isReady()&&!client.isBusy(),"changing workbook disconnects its conversation");
 client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"new workbook ready");client.send("New workbook",book);require(until([&]{return !client.isBusy()||!error.isEmpty();}),"new workbook turn");
 int newStarts=0,resumes=0;for(const auto& m:messages(script)){if(m.value("method",std::string())=="thread/start")++newStarts;if(m.value("method",std::string())=="thread/resume")++resumes;}require(newStarts==2&&resumes==0,"a new workbook never resumes the previous workbook thread");
}
void paginatedCatalog() {
 QTemporaryDir dir;CodexClient client;QString error;client.onError=[&](QString message){error=message;};
 client.connectRuntime(fakeRuntime(dir,"paged"));require(until([&]{return client.isReady()||!error.isEmpty();}),"catalog timeout");
 require(error.isEmpty(),qPrintable(error));require(client.models().size()==2,"all model catalog pages must be discovered");
 require(client.model()=="available-model","default model on a later page must be selected");
}
void suiteModelCatalog() {
 QTemporaryDir dir;CodexClient client;QString error;client.onError=[&](QString message){error=message;};
 client.connectRuntime(fakeRuntime(dir,"settings"));require(until([&]{return client.isReady()||!error.isEmpty();}),"settings catalog timeout");
 require(error.isEmpty(),qPrintable(error));require(client.models().size()==2,"hidden models must not appear in suite controls");
 require(client.model()=="gpt-6.1-sol","suite model picker prefers the advertised GPT-6.1 Sol");
 require(client.models()[0].at("model")=="other","runtime default models appear first in catalog");
}
void suiteSettingsSelection() {
 QTemporaryDir dir;CodexClient client;QString error;int catalogs=0;
 client.onError=[&](QString message){error=message;};client.onModelsChanged=[&]{++catalogs;};
 const auto script=fakeRuntime(dir,"settings");client.connectRuntime(script);
 require(until([&]{return client.isReady()||!error.isEmpty();}),"settings ready");require(error.isEmpty(),qPrintable(error));
 require(catalogs==1,"completed catalog discovery emits one control update");
 require(client.reasoningEffort()=="low","suite defaults to Light when advertised");
 require(client.serviceTier().isEmpty(),"Fast never activates merely because it is the first tier");
 client.setReasoningEffort("ultra");client.setServiceTier("fast");
 require(client.reasoningEffort()=="ultra"&&client.serviceTier()=="fast","advertised reasoning and speed can be selected");
 client.setReasoningEffort("invented");client.setServiceTier("priority");
 require(client.reasoningEffort()=="ultra"&&client.serviceTier()=="fast","unadvertised settings cannot be selected");
 client.setModel("other");
 require(client.model()=="other"&&client.reasoningEffort()=="medium"&&client.serviceTier()=="standard","model changes reset choices to the suite defaults");
 client.setModel("not-advertised");require(client.model()=="other","unknown model changes are ignored");
 require(catalogs==1,"model and setting selection do not republish the runtime catalog");
 for(const auto& message:messages(script))require(message.value("method",std::string())!="thread/start"&&message.value("method",std::string())!="turn/start","selecting controls does not send workbook context");
}
void turnSettingsSnapshot() {
 QTemporaryDir dir;CodexClient client;QString error;client.onError=[&](QString message){error=message;};
 const auto script=fakeRuntime(dir,"settings");client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"wire settings ready");
 client.setReasoningEffort("ultra");client.setServiceTier("fast");
 client.onStatus=[&](QString status){if(status=="Working…"){client.setModel("other");client.setReasoningEffort("high");}};
 const Json book={{"revision",7},{"sheets",Json::array()}};
 client.send("First",book);require(until([&]{return !client.isBusy()||!error.isEmpty();}),"first settings turn");require(error.isEmpty(),qPrintable(error));
 client.onStatus={};client.setServiceTier("");
 client.send("Second",book);require(until([&]{return !client.isBusy()||!error.isEmpty();}),"second settings turn");require(error.isEmpty(),qPrintable(error));
 Json turns=Json::array();for(const auto& message:messages(script)){
  if(message.value("method",std::string())=="turn/start")turns.push_back(message.at("params"));
  if(message.value("method",std::string())=="thread/start")require(message.at("params").value("model",std::string())=="gpt-6.1-sol"&&message.at("params").value("serviceTier",std::string())=="fast","thread uses settings captured before callbacks can change the controls");
 }
 require(turns.size()==2,"two settings turns are observed");
 require(turns[0].value("model",std::string())=="gpt-6.1-sol"&&turns[0].value("effort",std::string())=="ultra"&&turns[0].value("serviceTierForTurn",std::string())=="fast","first turn uses its captured model, reasoning and Fast settings");
 require(turns[1].value("model",std::string())=="other"&&turns[1].value("effort",std::string())=="high"&&turns[1].value("serviceTierForTurn",std::string())=="default","later turn applies changed settings and explicitly returns to Standard");
}
void disconnectAndResumeSettings() {
 QTemporaryDir dir;CodexClient client;QString error;client.onError=[&](QString message){error=message;};
 const auto script=fakeRuntime(dir,"settings-cancel");const Json book={{"revision",7},{"sheets",Json::array()}};
 client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"disconnect runtime ready");
 client.setModel("other");client.setReasoningEffort("high");
 client.send("Wait",book);require(until([&]{for(const auto& m:messages(script))if(m.value("method",std::string())=="turn/start")return true;return false;}),"turn before disconnect");
 client.disconnectRuntime();require(!client.isReady()&&!client.isBusy(),"Disconnect stops the process and frees the composer");
 client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"resume runtime ready");
 require(client.model()=="gpt-6.1-sol"&&client.reasoningEffort()=="low"&&client.serviceTier().isEmpty(),"reconnecting restores the same suite defaults as OwlDocs catalog discovery");
 client.setModel("other");client.setReasoningEffort("high");client.setServiceTier("");client.send("Continue",book);
 require(until([&]{for(const auto& m:messages(script))if(m.value("method",std::string())=="thread/resume")return true;return false;}),"disconnect keeps only this workbook's in-memory thread for resume");
 Json resumed;for(const auto& m:messages(script))if(m.value("method",std::string())=="thread/resume")resumed=m.at("params");
 require(resumed.value("model",std::string())=="other"&&resumed.value("serviceTier",std::string())=="default","resumed thread receives the currently selected model and Standard tier");
 require(resumed.at("config").at("mcp_servers").at("external").at("enabled")==false,"resuming selected settings keeps inherited MCP isolation");
 require(until([&]{int count=0;for(const auto& m:messages(script))if(m.value("method",std::string())=="turn/start")++count;return count==2;}),"resumed turn starts");
 Json last;for(const auto& m:messages(script))if(m.value("method",std::string())=="turn/start")last=m.at("params");
 require(last.value("effort",std::string())=="high"&&last.value("serviceTierForTurn",std::string())=="default","resumed turn receives its selected reasoning and Standard override");
 client.cancel();require(until([&]{return !client.isBusy();}),"resumed settings turn cancels");require(error.isEmpty(),qPrintable(error));
}
void removedCapabilitiesUseRuntimeDefaults() {
 QTemporaryDir dir;CodexClient client;QString error;client.onError=[&](QString message){error=message;};
 client.connectRuntime(fakeRuntime(dir,"settings"));require(until([&]{return client.isReady()||!error.isEmpty();}),"capabilities ready");client.setReasoningEffort("ultra");client.setServiceTier("fast");
 client.connectRuntime(fakeRuntime(dir,"empty"));require(until([&]{return client.isReady()||!error.isEmpty();}),"empty catalog ready");require(error.isEmpty(),qPrintable(error));
 require(client.model().isEmpty()&&client.reasoningEffort().isEmpty()&&client.serviceTier().isEmpty(),"removed runtime capabilities never retain a previously selected Fast or reasoning override");
}
void failureAndCancellation() {
 QTemporaryDir dir;CodexClient denied;QString deniedError;denied.onError=[&](QString message){deniedError=message;};const auto bad=fakeRuntime(dir,"badconfig");denied.connectRuntime(bad);
 require(until([&]{return !deniedError.isEmpty();}),"unknown MCP config must fail");require(!denied.isReady(),"unsafe runtime cannot be ready");
 for(const auto& m:messages(bad))require(m.value("method",std::string())!="account/read","unrecognized configuration must fail closed before auth");
 CodexClient client;QString error;client.onError=[&](QString message){error=message;};const auto script=fakeRuntime(dir,"cancel");client.connectRuntime(script);
 require(until([&]{return client.isReady()||!error.isEmpty();}),"cancel runtime ready");
 const Json book={{"revision",0},{"sheets",Json::array()}};
 client.send("Wait",book);require(until([&]{for(const auto& m:messages(script))if(m.value("method",std::string())=="turn/start")return true;return false;}),"turn was sent");
 client.cancel();require(until([&]{return !client.isBusy();}),"cancel must finish the turn");require(error.isEmpty(),qPrintable(error));
 client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"reconnect ready");client.send("Continue",book);
 require(until([&]{for(const auto& m:messages(script))if(m.value("method",std::string())=="thread/resume")return true;return false;}),"saved in-memory thread must resume");
 for(const auto& m:messages(script))if(m.value("method",std::string())=="thread/resume")require(m.at("params").at("config").at("mcp_servers").at("external").at("enabled")==false,"resumed thread retains MCP isolation");
 client.cancel();require(until([&]{return !client.isBusy();}),"resumed turn canceled");
}
void schemaFile() {
 const auto path=QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../schemas/workbook.v1.json");QFile file(path);
 require(file.open(QIODevice::ReadOnly),"workbook.v1 tool schema must ship with TigerSheets");const auto schema=Json::parse(file.readAll().toStdString());
 require(schema.at("tools")==CodexClient::toolDefinitions(),"published workbook schema must match registered dynamic tools");
}
void lateCallsAfterStop() {
 QTemporaryDir dir;CodexClient client;QString error,output;int tools=0;client.onError=[&](QString message){error=message;};client.onTool=[&](const QString&,const Json&){++tools;return Json::object();};client.onText=[&](QString delta){output+=delta;};const auto script=fakeRuntime(dir,"late-cancel");client.connectRuntime(script);require(until([&]{return client.isReady()||!error.isEmpty();}),"late cancel runtime ready");client.send("Wait",{{"revision",7},{"sheets",Json::array()}});require(until([&]{for(const auto& m:messages(script))if(m.value("method",std::string())=="turn/start")return true;return false;}),"late cancel turn starts");client.cancel();require(until([&]{return !client.isBusy();}),"late cancel turn finishes");require(tools==0&&output.isEmpty(),"Stop rejects late workbook tools and streamed text before interrupted completion");require(error.isEmpty(),qPrintable(error));
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);qputenv("TIGERSHEETS_TEST_PRIVATE_SECRET","must-not-reach-runtime");try{handshake();conversation();paginatedCatalog();suiteModelCatalog();suiteSettingsSelection();turnSettingsSnapshot();disconnectAndResumeSettings();removedCapabilitiesUseRuntimeDefaults();failureAndCancellation();schemaFile();lateCallsAfterStop();std::cout<<"codex tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
