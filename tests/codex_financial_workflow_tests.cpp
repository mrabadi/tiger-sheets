#include "tigersheets/main_window.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/chart.hpp"
#include "tigersheets/codex.hpp"
#include "tigersheets/ooxml.hpp"
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QTest>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool until(const std::function<bool()>& predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,10);return predicate();}
QAbstractButton* button(QWidget* parent,const QString& text){for(auto* candidate:parent->findChildren<QAbstractButton*>())if(candidate->text()==text)return candidate;return nullptr;}
ChartWidget* figure(MainWindow& window){for(auto* widget:window.findChildren<QWidget*>())if(auto* chart=dynamic_cast<ChartWidget*>(widget))return chart;return nullptr;}
Json cell(const Json& book,int row,int col){for(const auto& value:book.at("sheets")[0].at("cells"))if(value.at("row")==row&&value.at("col")==col)return value;return Json::object();}
Json withoutRevision(Json book){book.erase("revision");return book;}
QString fakeFinancialRuntime(QTemporaryDir& directory){
 const auto path=directory.filePath("codex-financial");QFile file(path);require(file.open(QIODevice::WriteOnly),"financial fake executable writable");
 file.write(R"PY(#!/usr/bin/python3
import json, os, sys
log=open(__file__+'.log','a',buffering=1)
revision=None
sheet=None
def emit(message): print(json.dumps(message),flush=True)
def reply(message,result): emit({'id':message['id'],'result':result})
def tool(identifier,name,arguments): emit({'id':identifier,'method':'item/tool/call','params':{'threadId':'financial-thread','turnId':'financial-turn','callId':identifier,'tool':name,'arguments':arguments}})
def fail(message):
 with open(__file__+'.failure.pending','w') as result: result.write(message)
 os.replace(__file__+'.failure.pending',__file__+'.failure')
 raise RuntimeError(message)
def check(condition,message):
 if not condition: fail(message)
for line in sys.stdin:
 m=json.loads(line);log.write(json.dumps(m)+'\n');method=m.get('method');p=m.get('params',{})
 if method=='initialize': reply(m,{'userAgent':'financial-fixture','platformOs':'linux'})
 elif method=='initialized': pass
 elif method=='config/read': reply(m,{'config':{'mcp_servers':{}}})
 elif method=='account/read': reply(m,{'account':{'type':'chatgpt'},'requiresOpenaiAuth':True})
 elif method=='model/list': reply(m,{'data':[{'id':'fixture','model':'fixture','isDefault':True,'supportedReasoningEfforts':['low'],'serviceTiers':['default','fast']}],'nextCursor':None})
 elif method=='thread/start':
  check(p.get('sandbox')=='read-only' and p.get('approvalPolicy')=='never','financial authoring must retain desktop sandbox restrictions')
  names=[t['name'] for t in p.get('dynamicTools',[])]
  check('workbook_v1_preview' in names,'workbook preview tool must be registered')
  reply(m,{'thread':{'id':'financial-thread'}})
 elif method=='turn/start':
  text=p['input'][0]['text']
  check('Can you generate some realistic financial data, put it into a financial model, and then create a graph of forecasted revenue for that fictitious company/model?' in text,'the exact reported composer prompt must reach the worker')
  reply(m,{'turn':{'id':'financial-turn','status':'inProgress'}})
  tool('capabilities','workbook_v1_capabilities',{})
 elif method is None and 'result' in m:
  check(m['result'].get('success',False),'workbook tool '+str(m['id'])+' failed: '+json.dumps(m['result']))
  data=json.loads(m['result']['contentItems'][0]['text'])
  if m['id']=='capabilities':
   check(data.get('version')=='workbook.v1' and isinstance(data.get('operationSchemas'),list),'capabilities must publish versioned operation schemas for workbook authoring')
   schemas=data['operationSchemas'];check(any('set_range' in json.dumps(s) for s in schemas) and any('add_chart' in json.dumps(s) for s in schemas),'capabilities must document range and chart authoring')
   check('financialModel' in data.get('examples',{}),'capabilities must include a usable financial model example')
   tool('initial-read','workbook_v1_read',{'row1':1,'col1':1,'row2':7,'col2':5})
  elif m['id']=='initial-read':
   revision=data['revision'];sheet=data['sheets'][0]['id']
   values=[['Period','Revenue','EBITDA','Assumption','Value'],[2024,12000000,'=B2*$E$3','Revenue growth',0.18],[2025,15000000,'=B3*$E$3','EBITDA margin',0.22],[2026,'=B3*(1+$E$2)','=B4*$E$3','Company','Cedar Ridge Analytics (fictional)'],[2027,'=B4*(1+$E$2)','=B5*$E$3',None,None],[2028,'=B5*(1+$E$2)','=B6*$E$3',None,None],['Historical values are illustrative; forecasts use explicit assumptions.',None,None,None,None]]
   ops=[{'type':'rename_sheet','sheet':sheet,'name':'Cedar Ridge Analytics'},{'type':'set_range','sheet':sheet,'row':1,'col':1,'values':values}]
   for col in range(1,6): ops.append({'type':'set_style','sheet':sheet,'row':1,'col':col,'style':{'bold':True,'fill':'#e7efe6'}})
   for row in range(2,7):
    for col in (2,3): ops.append({'type':'set_style','sheet':sheet,'row':row,'col':col,'style':{'numberFormat':'$#,##0'}})
   for row in (2,3): ops.append({'type':'set_style','sheet':sheet,'row':row,'col':5,'style':{'numberFormat':'0.0%'}})
   ops.append({'type':'add_sheet','id':'native-report','name':'Revenue Report'})
   ops.append({'type':'create_pivot','definition':{'id':'forecast-report','name':'ForecastReport','source':{'sheet':sheet,'row1':1,'col1':1,'row2':6,'col2':2},'rows':[{'field':'Period'}],'values':[{'field':'Revenue','aggregate':'sum','name':'Revenue'}],'output':{'sheet':'native-report','row1':1,'col1':1}}})
   tool('model-preview','workbook_v1_preview',{'revision':revision,'operations':ops})
  elif m['id']=='model-preview':
   check(data.get('status')=='preview','financial model must be a reviewable preview')
   chart={'id':'revenue-forecast','sheet':sheet,'type':'line','title':'Cedar Ridge Analytics revenue forecast','range':{'row1':1,'col1':1,'row2':6,'col2':2},'anchor':{'row':9,'col':1,'width':560,'height':300}}
   tool('chart-preview','workbook_v1_preview',{'revision':revision,'operations':[{'type':'add_chart','chart':chart}]})
  elif m['id']=='chart-preview':
   check(data.get('status')=='preview','chart must join the existing reviewable preview')
   tool('pending-read','workbook_v1_read',{'sheet':sheet,'row1':1,'col1':1,'row2':7,'col2':5})
  elif m['id']=='pending-read':
   check(data.get('view')=='preview','reads during authoring must inspect pending edits')
   cells=data['sheets'][0]['cells'];check(any(c.get('row')==4 and c.get('col')==2 and c.get('input')=='=B3*(1+$E$2)' for c in cells),'second preview must retain the financial model formulas')
   check(any(c.get('row')==4 and c.get('col')==2 and c.get('value')==17700000 for c in cells),'Codex must be able to read calculated pending forecast values')
   check(any(c.get('id')=='revenue-forecast' for c in data.get('charts',[])),'pending read must include the chart created by the second preview')
   tool('pending-search','workbook_v1_search',{'query':'Cedar Ridge Analytics'})
  elif m['id']=='pending-search':
   check(data.get('view')=='preview' and len(data.get('matches',[]))>0,'search must find pending company data')
   emit({'method':'item/agentMessage/delta','params':{'threadId':'financial-thread','turnId':'financial-turn','delta':'Financial model and forecast revenue graph are ready for your review.'}})
   emit({'method':'turn/completed','params':{'threadId':'financial-thread','turn':{'id':'financial-turn','status':'completed'}}})
 else: reply(m,{})
)PY");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;
}
void financialAuthoringWorkflow(){
 QTemporaryDir directory;MainWindow window;if(!qEnvironmentVariableIsEmpty("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"))window.setProperty("reportWorkerExecutable",qEnvironmentVariable("TIGERSHEETS_REPORT_WORKER_TEST_EXECUTABLE"));window.resize(1440,1000);window.show();
 auto* dock=window.findChild<QDockWidget*>("workspace.codex");require(dock,"Codex dock exists");dock->show();
 CodexClient* client=nullptr;for(auto* object:window.findChildren<QObject*>())if(auto* candidate=dynamic_cast<CodexClient*>(object))client=candidate;
 require(client,"desktop Codex client exists");const auto script=fakeFinancialRuntime(directory);client->connectRuntime(script);require(until([&]{return client->isReady();}),"financial fake runtime connects");
 auto* composer=dock->findChild<QPlainTextEdit*>();auto* send=button(dock,"Send");require(composer&&send&&send->isEnabled(),"connected native composer is usable");
 const auto initial=window.currentSession().snapshot();composer->setPlainText("Can you generate some realistic financial data, put it into a financial model, and then create a graph of forecasted revenue for that fictitious company/model?");send->click();
 require(until([&]{return QFile::exists(script+".failure")||!client->isBusy();}),"financial authoring turn completes");
 QFile failure(script+".failure");if(failure.open(QIODevice::ReadOnly))throw std::runtime_error(failure.readAll().toStdString());
 auto* transcript=dock->findChild<QTextBrowser*>();require(transcript&&transcript->toPlainText().contains("ready for your review"),"streamed response describes the reviewable workbook result");
 require(transcript->toPlainText().contains("Codex\nFinancial model and forecast revenue graph are ready for your review."),"streamed assistant text must start on its own line below the Codex role");
 require(window.currentSession().hasPreview()&&window.currentSession().snapshot()==initial,"generated model and chart stay isolated before Accept");
 auto* accept=button(&window,"Accept");require(accept&&accept->isVisible(),"desktop presents the Accept control");accept->click();
 const auto accepted=window.currentSession().snapshot();require(!window.currentSession().hasPreview(),"Accept completes the proposal");
 require(accepted.at("sheets")[0].at("name")=="Cedar Ridge Analytics"&&cell(accepted,4,2).value("input",std::string())=="=B3*(1+$E$2)","accepted financial model retains company, historical data, assumptions and formulas");
 require(cell(accepted,1,2).at("style").value("bold",false)&&cell(accepted,2,2).at("style").value("numberFormat",std::string())=="$#,##0","accepted model retains header and financial number formatting");
 require(accepted.at("pivots")[0].at("layout").at("grandTotals")[0].at("value").get<double>()>90000000,"one proposal calculates newly authored formula source before generating a native report");
 require(accepted.at("charts").size()==1&&accepted.at("charts")[0].at("id")=="revenue-forecast","second proposal adds its chart without losing the model");
 const auto calculated=CalculationEngine().calculate(accepted);const auto sheet=accepted.at("sheets")[0].at("id");
 for(const auto& expected:{std::pair{4,17700000.0},std::pair{5,20886000.0},std::pair{6,24645480.0}}){bool found=false;for(const auto& value:calculated.at("values"))if(value.at("sheet")==sheet&&value.at("row")==expected.first&&value.at("col")==2){found=value.at("type")=="number"&&std::abs(value.at("value").get<double>()-expected.second)<0.001;}require(found,"forecast revenue must recalculate from the explicit 18 percent growth assumption");}
 const auto chartValues=chartData(accepted,calculated,accepted.at("charts")[0]);require(std::abs(chartValues.at("series")[0].at("values")[4].get<double>()-24645480.0)<0.001,"native graph uses calculated forecast revenue");
 require(window.executeCommand("undo")&&withoutRevision(window.currentSession().snapshot())==withoutRevision(initial)&&!window.currentSession().canUndo(),"one Undo removes the entire model and chart accepted together");
 require(window.executeCommand("redo"),"accepted model and graph can be restored together");window.recalculateNow();auto* chart=figure(window);require(chart&&chart->isEnabled(),"accepted forecast graph is a native editable figure");
 QTest::mouseClick(chart,Qt::LeftButton,Qt::NoModifier,QPoint(90,25));require(chart->isSelected(),"generated graph responds to selection");bool editorVisited=false;
 QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QDialog*>("chartEditor")){editorVisited=true;dialog->findChild<QLineEdit*>("chartTitle")->setText("Revenue forecast reviewed");dialog->findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok)->click();}});
 QTest::mouseDClick(chart,Qt::LeftButton,Qt::NoModifier,QPoint(90,25));require(editorVisited&&window.currentSession().snapshot().at("charts")[0].at("title")=="Revenue forecast reviewed","generated graph opens the normal chart editor and accepts changes");
 auto* grid=window.findChild<QTableView*>("worksheet.grid");
 require(grid&&until([&]{return grid->model()->data(grid->model()->index(3,1)).toString()=="$17,700,000";}),"the worksheet must visibly display calculated forecast revenue after chart editing");
 const auto path=directory.filePath("Cedar-Ridge-Analytics.xlsx");require(window.saveWorkbook(path),"accepted financial model saves as XLSX");
 MainWindow reopened;require(reopened.openWorkbook(path),"saved financial model reopens in TigerSheets");const auto persisted=reopened.currentSession().snapshot();
 require(cell(persisted,4,2).value("input",std::string())=="=B3*(1+$E$2)"&&cell(persisted,2,2).value("input",std::string())=="12000000","XLSX round trip retains forecast formulas and historical financial data");
 require(cell(persisted,1,2).at("style").value("bold",false)&&persisted.at("charts").size()==1&&persisted.at("charts")[0].at("title")=="Revenue forecast reviewed","XLSX round trip retains formatting and the edited graph");
 if(!qEnvironmentVariableIsEmpty("TIGERSHEETS_CODEX_FINANCIAL_XLSX"))require(QFile::copy(path,qEnvironmentVariable("TIGERSHEETS_CODEX_FINANCIAL_XLSX")),"financial workflow fixture saves");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{if(argc<2||std::string(argv[1])=="financial")financialAuthoringWorkflow();std::cout<<"Codex financial workflow passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
