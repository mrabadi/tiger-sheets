#include "tigersheets/main_window.hpp"
#include "tigersheets/codex.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/pivot.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTableView>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool until(const std::function<bool()>& predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,10);return predicate();}
QString executable(QTemporaryDir& directory,const char* name,const char* source){const auto path=directory.filePath(name);QFile file(path);require(file.open(QIODevice::WriteOnly),"write deterministic executable");file.write(source);file.close();require(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"private executable permissions");return path;}
QString runtime(QTemporaryDir& directory){return executable(directory,"codex-host",R"PY(#!/usr/bin/python3
import json,sys
def emit(v): print(json.dumps(v),flush=True)
for line in sys.stdin:
 m=json.loads(line);method=m.get('method')
 if method=='initialized': continue
 if method is None:
  with open(__file__+'.reply','w') as f: json.dump(m,f)
  emit({'method':'turn/completed','params':{'threadId':'host-thread','turn':{'id':'host-turn','status':'completed'}}})
  continue
 result={}
 if method=='initialize': result={'userAgent':'host-test','platformOs':'linux'}
 elif method=='config/read': result={'config':{'mcp_servers':{}}}
 elif method=='account/read': result={'account':{'type':'chatgpt'},'requiresOpenaiAuth':True}
 elif method=='model/list': result={'data':[{'id':'fixture','model':'fixture','isDefault':True,'supportedReasoningEfforts':['low']}],'nextCursor':None}
 elif method=='thread/start': result={'thread':{'id':'host-thread'}}
 elif method=='turn/start': result={'turn':{'id':'host-turn','status':'inProgress'}}
 emit({'id':m['id'],'result':result})
 if method=='turn/start': emit({'id':'host-tool','method':'item/tool/call','params':{'threadId':'host-thread','turnId':'host-turn','tool':'workbook_v1_capabilities','arguments':{}}})
 if method=='turn/interrupt': emit({'method':'turn/completed','params':{'threadId':'host-thread','turn':{'id':'host-turn','status':'interrupted'}}})
)PY");}
QString worker(QTemporaryDir& directory){return executable(directory,"report-worker",R"PY(#!/usr/bin/python3
import json,sys,time
args=dict(zip(sys.argv[1::2],sys.argv[2::2]))
with open(args['--request']) as f: request=json.load(f)
with open(__file__+'.started','w') as f: f.write('started')
time.sleep(0.4)
book=request['workbook'];sheet=book['sheets'][0]['id']
result={'version':1,'kind':'workbook_tool','revision':book['revision'],'contextGeneration':request['contextGeneration'],'workbookId':request['workbookId'],'status':'ok','diagnostics':[],'toolReply':{'status':'preview','revision':book['revision']},'operations':[{'type':'set_cell','sheet':sheet,'row':2,'col':2,'input':'42'}]}
with open(args['--response'],'w') as f: json.dump(result,f)
)PY");}
CodexClient* clientFor(MainWindow& window){for(auto* child:window.children())if(auto* client=dynamic_cast<CodexClient*>(child))return client;throw std::runtime_error("desktop Codex client");}
void begin(MainWindow& window,CodexClient* client,const QString& script){client->connectRuntime(script);require(until([&]{return client->isReady();}),"connect deterministic runtime");auto* composer=window.findChild<QPlainTextEdit*>("codex.input");auto* send=window.findChild<QPushButton*>("codex.send");require(composer&&send,"desktop composer controls");composer->setPlainText("Prepare a fictional report preview");send->click();}
void cancellableHost(){QTemporaryDir directory;MainWindow window;const auto script=runtime(directory),child=worker(directory);window.setProperty("reportWorkerExecutable",child);auto* client=clientFor(window);const auto initial=window.currentSession().snapshot();begin(window,client,script);require(until([&]{return QFile::exists(child+".started")||!client->isBusy();})&&QFile::exists(child+".started"),"desktop workbook tools must execute in an isolated cancellable child");bool tick=false;QTimer::singleShot(0,&window,[&]{tick=true;});require(until([&]{return tick;}),"desktop event loop remains responsive during isolated calculation");QElapsedTimer latency;latency.start();client->cancel();require(latency.elapsed()<100,"Stop returns within 100 ms");require(until([&]{return !client->isBusy()&&!window.property("codexToolBusy").toBool();}),"Stop clears the isolated job");QElapsedTimer settle;settle.start();while(settle.elapsed()<700)QApplication::processEvents(QEventLoop::AllEvents,10);require(!window.currentSession().hasPreview()&&window.currentSession().snapshot()==initial,"cancelled child never publishes a preview");}
void staleHost(){QTemporaryDir directory;MainWindow window;const auto script=runtime(directory),child=worker(directory);window.setProperty("reportWorkerExecutable",child);auto* client=clientFor(window);begin(window,client,script);require(until([&]{return QFile::exists(child+".started")||!client->isBusy();})&&QFile::exists(child+".started"),"isolated worker started before revision edit");const auto& book=window.currentSession().snapshotView();std::string error;require(window.currentSession().apply(book.at("revision"),Json::array({{{"type","set_cell"},{"sheet",book.at("sheets")[0].at("id")},{"row",1},{"col",1},{"input","new revision"}}}),&error),"edit workbook while worker runs");const auto edited=window.currentSession().snapshot();require(until([&]{return !client->isBusy();}),"stale tool completion finishes");require(!window.currentSession().hasPreview()&&window.currentSession().snapshot()==edited,"foreign-revision worker result is discarded atomically");QFile reply(script+".reply");require(reply.open(QIODevice::ReadOnly),"stale tool response exists");const auto message=Json::parse(reply.readAll().toStdString());require(!message.at("result").value("success",true),"stale tool result fails the protocol call");}
void reportStatus(){MainWindow window;auto& session=window.currentSession();Json operations=Json::array({{{"type","add_sheet"},{"id","report"},{"name","Report"}}});const std::vector<std::vector<std::string>> rows={{"Region","Revenue"},{"East","12"},{"West","18"}};for(int row=0;row<3;++row)for(int col=0;col<2;++col)operations.push_back({{"type","set_cell"},{"sheet","sheet-1"},{"row",row+1},{"col",col+1},{"input",rows[row][col]}});require(session.apply(0,operations),"prepare report fixture");const auto definition=Json::parse(R"json({"id":"status-report","name":"StatusReport","source":{"sheet":"sheet-1","row1":1,"col1":1,"row2":3,"col2":2},"rows":[{"field":"Region"}],"values":[{"field":"Revenue","aggregate":"sum","name":"Revenue"}],"output":{"sheet":"report","row1":1,"col1":1}})json");const auto generated=generatePivot(session.snapshotView(),{{"revision",session.snapshotView().at("revision")},{"status","ok"},{"values",Json::array()}},definition);require(session.apply(session.snapshotView().at("revision"),pivotOperations(session.snapshotView(),generated)),"commit report fixture");auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());model->refresh();require(window.executeCommand("pivot.fields",{{"id","status-report"}}),"open native report fields");auto* status=window.findChild<QLabel*>("pivot.status");require(status&&status->text().contains("current"),"fresh report status is visible");require(model->setData(model->index(1,1),"99",Qt::EditRole),"edit native report source through the grid model");require(status->text().contains("Needs refresh"),"source edits immediately update the visible report refresh status");}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{if(argc<2||std::string(argv[1])=="host"){cancellableHost();staleHost();}if(argc<2||std::string(argv[1])=="status")reportStatus();std::cout<<"Codex isolated host tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
