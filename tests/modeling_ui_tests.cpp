#include "tigersheets/modeling_ui.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QTimer>
#include <QTableView>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json simpleModel(){return Json::parse(R"json({"revision":0,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"1"},{"row":1,"col":2,"input":"=A1*2"}]}]})json");}
void goalSeekAction(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);int changes=0;model.onChanged=[&]{++changes;};
 require(modelingCommand(nullptr,session,model,view,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",10}}),"Goal Seek action must apply its converged workbook result");
 require(std::abs(model.raw(0,0).toDouble()-5)<1e-7,"Goal Seek must solve actual formula B1=A1*2");
 require(session.snapshot().at("revision")==1&&changes==1,"Goal Seek is one observable transaction");
 require(session.undo(),"Goal Seek must be undoable");model.refresh();require(model.raw(0,0)=="1","One undo restores original changing cell");
 const auto before=session.snapshot();
 require(!modelingCommand(nullptr,session,model,view,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",20},{"accept",false}}),"Declined Goal Seek result returns false");
 require(session.snapshot()==before,"Declining Goal Seek must leave workbook unchanged");
}
void linkedAnalysisUsesFrozenContext(){
 auto book=simpleModel();book["sheets"][0]["cells"][1]["input"]="='[Drivers.xlsx]Inputs'!A1*A1";
 WorkbookSession session(book);WorksheetModel model(session);QTableView view;view.setModel(&model);
 CalculationContext context;context.snapshot=book;context.generation=7;
 context.snapshot["links"]=Json::parse(R"json([{"name":"Drivers.xlsx","path":"/not-read/Drivers.xlsx","enabled":true,"workbook":{"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"7"}]}]}}])json");
 context.generationIsCurrent=[](std::uint64_t generation){return generation==7;};
 require(modelingCommand(nullptr,session,model,view,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",14}},&context),"Goal Seek must use the same frozen linked source snapshot as the worksheet");
 require(std::abs(model.raw(0,0).toDouble()-2)<1e-7,"Linked Goal Seek must solve the actual qualified model");
 const auto before=session.snapshot();context.snapshot=before;context.generationIsCurrent=[](std::uint64_t){return false;};bool rejected=false;
 try{modelingCommand(nullptr,session,model,view,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",21}},&context);}catch(const std::exception&){rejected=true;}
 require(rejected&&session.snapshot()==before,"A replaced calculation context must refuse an analysis result atomically");
}
void solverAction(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);int changes=0;model.onChanged=[&]{++changes;};
 Json spec={{"objective",{{"sheet","m"},{"row",1},{"col",2}}},{"goal","max"},{"method","linear"},{"variables",Json::array({{{"sheet","m"},{"row",1},{"col",1},{"lower",0},{"upper",4}}})},{"constraints",Json::array()},{"maxEvaluations",100}};
 require(modelingCommand(nullptr,session,model,view,"analysis.solver",spec),"Solver action must apply its real feasible solution");
 require(model.raw(0,0).toDouble()==4&&changes==1&&session.snapshot().at("revision")==1,"Solver accepts changing cells in one observable edit");
 const auto before=session.snapshot();spec["goal"]="min";spec["accept"]=false;
 require(!modelingCommand(nullptr,session,model,view,"analysis.solver",spec)&&session.snapshot()==before,"Declined Solver candidate must not affect workbook");
}

void scenarioActions(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);int changes=0;model.onChanged=[&]{++changes;};
 require(modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","save"},{"name","Base"},{"range","A1"}}),"Scenario manager saves selected constant inputs");
 require(session.snapshot().at("scenarios").size()==1&&changes==1,"Saved scenario is workbook metadata in one edit");
 model.setData(model.index(0,0),7,Qt::EditRole);
 require(modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","apply"},{"name","Base"}}),"Scenario manager applies captured input cells");
 require(model.raw(0,0)=="1"&&model.raw(0,1)=="=A1*2"&&changes==3,"Scenario apply restores constants and preserves formulas in one edit");
 require(session.undo(),"Scenario apply is one undo");model.refresh();require(model.raw(0,0)=="7","One undo restores pre-scenario input");
 const auto before=session.snapshot();require(!modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","apply"},{"name","Base"},{"accept",false}})&&session.snapshot()==before,"Declined scenario apply preserves live workbook");
 require(modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","delete"},{"name","Base"}})&&session.snapshot().at("scenarios").empty(),"Scenario manager deletes a saved definition");
}

void scenariosPreserveNewFormulas(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);
 require(modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","save"},{"name","Base"},{"range","A1"}}),"Save the original scenario inputs");
 model.setData(model.index(0,0),"=3+4",Qt::EditRole);const auto before=session.snapshot();bool rejected=false;
 try{modelingCommand(nullptr,session,model,view,"scenario.manage",{{"action","apply"},{"name","Base"}});}catch(const std::exception&){rejected=true;}
 require(rejected&&session.snapshot()==before,"Applying a saved scenario must refuse newly introduced destination formulas atomically");
}
void pivotAction(){
 const auto book=Json::parse(R"json({"revision":0,"sheets":[{"id":"m","name":"Sales","cells":[{"row":1,"col":1,"input":"Region"},{"row":1,"col":2,"input":"Revenue"},{"row":2,"col":1,"input":"East"},{"row":2,"col":2,"input":"12"},{"row":3,"col":1,"input":"West"},{"row":3,"col":2,"input":"7"},{"row":4,"col":1,"input":"East"},{"row":4,"col":2,"input":"8"}]}]})json");
 WorkbookSession session(book);WorksheetModel model(session);QTableView view;view.setModel(&model);int changes=0;model.onChanged=[&]{++changes;};
 require(modelingCommand(nullptr,session,model,view,"pivot.create",{{"range","A1:B4"},{"rowField","Region"},{"valueField","Revenue"},{"aggregate","sum"},{"output","D1"}}),"Pivot builder must create a summary from selected header/data rows");
 require(model.data(model.index(1,3)).toString()=="East"&&model.raw(1,4).toDouble()==20,"Pivot groups actual source values");
 require(model.data(model.index(2,3)).toString()=="West"&&model.raw(2,4).toDouble()==7&&model.raw(3,4).toDouble()==27,"Pivot includes second group and grand total");
 require(session.snapshot().at("pivots").size()==1&&changes==1&&session.snapshot().at("revision")==1,"Pivot cells and definition are one transaction");
 require(model.raw(1,1)=="12","Pivot must preserve source records");require(session.undo(),"Pivot insertion is one undo");model.refresh();require(model.raw(0,3).isEmpty(),"Undo removes all summary output");
 require(modelingCommand(nullptr,session,model,view,"pivot.create",{{"range","A1:B4"},{"rowField","Region"},{"valueField","Revenue"},{"aggregate","average"},{"output","D1"}}),"Pivot dialog aggregate choice must execute native average");
 require(model.raw(1,4).toDouble()==10&&model.raw(3,4).toDouble()==9,"Pivot average grand total is weighted over original records");
}

void sensitivityAction(){
 const auto book=Json::parse(R"json({"revision":0,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"1"},{"row":1,"col":2,"input":"2"},{"row":1,"col":3,"input":"=A1*B1"}]}]})json");
 WorkbookSession session(book);WorksheetModel model(session);QTableView view;view.setModel(&model);int changes=0;model.onChanged=[&]{++changes;};
 const Json args={{"rowValues",Json::array({10,20})},{"colValues",Json::array({2,3})},{"rowInput","A1"},{"columnInput","B1"},{"output","C1"},{"destination","E10"}};
 require(modelingCommand(nullptr,session,model,view,"analysis.table",args),"Sensitivity action must calculate and insert every input combination");
 require(model.raw(9,5).toDouble()==10&&model.raw(9,6).toDouble()==20&&model.raw(10,4).toDouble()==2&&model.raw(11,4).toDouble()==3,"Sensitivity headers use Excel row/column orientation");
 require(model.raw(10,5).toDouble()==20&&model.raw(10,6).toDouble()==40&&model.raw(11,5).toDouble()==30&&model.raw(11,6).toDouble()==60,"Sensitivity cells must contain recalculated model outputs");
 require(model.raw(0,0)=="1"&&model.raw(0,1)=="2"&&model.raw(0,2)=="=A1*B1","Sensitivity trials must not alter model inputs or output formula");
 require(changes==1&&session.snapshot().at("revision")==1,"Sensitivity result is one observable transaction");require(session.undo(),"Sensitivity output is one undo");model.refresh();require(model.raw(9,4).isEmpty(),"Undo removes the complete sensitivity grid");
}

void udfModels(){
 auto book=simpleModel();book["vba"]={{"enabled",true},{"modules",Json::array({{{"name","ModelFunctions"},{"source","Function Triple(x As Double) As Double\nTriple = x * 3\nEnd Function"}}})}};book["sheets"][0]["cells"][1]["input"]="=Triple(A1)";
 WorkbookSession session(book);WorksheetModel model(session);QTableView view;view.setModel(&model);
 require(modelingCommand(nullptr,session,model,view,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",15}}),"Goal Seek must compose enabled read-only VBA worksheet functions");require(std::abs(model.raw(0,0).toDouble()-5)<1e-7,"Goal Seek UDF result");
 auto tableBook=book;tableBook["sheets"][0]["cells"][1]["input"]="2";tableBook["sheets"][0]["cells"].push_back({{"row",1},{"col",3},{"input","=MultiplyInputs(A1,B1)"}});tableBook["vba"]["modules"][0]["source"]="Function MultiplyInputs(lhs As Double, rhs As Double) As Double\nMultiplyInputs = lhs * rhs\nEnd Function";
 WorkbookSession tableSession(tableBook);WorksheetModel tableModel(tableSession);QTableView tableView;tableView.setModel(&tableModel);
 require(modelingCommand(nullptr,tableSession,tableModel,tableView,"analysis.table",{{"rowValues",Json::array({10,20})},{"colValues",Json::array({2,3})},{"rowInput","A1"},{"columnInput","B1"},{"output","C1"},{"destination","E10"}}),"Sensitivity table must compose enabled read-only VBA functions");require(tableModel.raw(11,6).toDouble()==60,"Sensitivity UDF calculation must use each trial snapshot");
 auto disabled=book;disabled["vba"]["enabled"]=false;WorkbookSession disabledSession(disabled);WorksheetModel disabledModel(disabledSession);QTableView disabledView;disabledView.setModel(&disabledModel);bool failed=false;try{modelingCommand(nullptr,disabledSession,disabledModel,disabledView,"analysis.goalSeek",{{"output","B1"},{"changing","A1"},{"target",15}});}catch(const std::exception&){failed=true;}require(failed&&disabledSession.snapshot()==disabled,"Disabled VBA must not be run by modeling actions");
}
void nativeDialogsAreTypedAndCancellable(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);
 const std::vector<std::pair<QString,QString>> commands={{"analysis.goalSeek","modeling.goalSeek"},{"analysis.solver","modeling.solver"},{"scenario.manage","modeling.scenarios"},{"pivot.create","modeling.pivot"},{"analysis.table","modeling.sensitivity"}};
 for(const auto& [command,name]:commands){bool inspected=false,typed=false;QTimer timer;timer.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;inspected=dialog->objectName()==name;if(command=="analysis.goalSeek")typed=dialog->findChild<QLineEdit*>("goalSeek.output")&&dialog->findChild<QDoubleSpinBox*>("goalSeek.target");else if(command=="analysis.solver")typed=dialog->findChild<QTableWidget*>("solver.variables")&&dialog->findChild<QTableWidget*>("solver.constraints")&&dialog->findChild<QComboBox*>("solver.method");else if(command=="scenario.manage")typed=dialog->findChild<QComboBox*>("scenario.action")&&dialog->findChild<QLineEdit*>("scenario.range");else if(command=="pivot.create")typed=dialog->findChild<QComboBox*>("pivot.rowField")&&dialog->findChild<QComboBox*>("pivot.aggregate");else typed=dialog->findChild<QLineEdit*>("table.rowInput")&&dialog->findChild<QLineEdit*>("table.rowValues");dialog->reject();});const auto before=session.snapshot();timer.start(0);require(!modelingCommand(nullptr,session,model,view,command),"Canceling native dialog returns false");timer.stop();require(inspected&&typed,"Modeling dialog exposes native typed fields");require(session.snapshot()==before,"Canceling any modeling dialog preserves workbook");}
}
void malformedArguments(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);bool rejected=false;QTimer timer;timer.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});timer.start(0);try{modelingCommand(nullptr,session,model,view,"analysis.goalSeek",Json("invalid"));}catch(const std::exception&){rejected=true;}timer.stop();require(rejected,"Malformed automation arguments must be rejected rather than opening an interactive dialog");
}

void failuresAreAtomic(){
 WorkbookSession session(simpleModel());WorksheetModel model(session);QTableView view;view.setModel(&model);
 const auto fails=[&](const QString& command,const Json& args,const std::string& message){const auto before=session.snapshot();std::string error;try{modelingCommand(nullptr,session,model,view,command,args);}catch(const std::exception& failure){error=failure.what();}require(error.find(message)!=std::string::npos,"Invalid modeling action must return a descriptive error");require(session.snapshot()==before,"Failed modeling action must not mutate workbook");};
 fails("analysis.goalSeek",{{"output","B1"},{"changing","B1"},{"target",10}},"numeric constant");
 fails("scenario.manage",{{"action","save"},{"name","Bad"},{"range","B1"}},"constants");
 fails("analysis.table",{{"rowValues",Json::array({1})},{"colValues",Json::array({2})},{"rowInput","A1"},{"columnInput","C1"},{"output","B1"},{"destination","A1"}},"overlaps");
 require(!modelingCommand(nullptr,session,model,view,"unrecognized.command"),"Unrecognized modeling command must return false without a dialog");
 auto duplicate=simpleModel();duplicate["sheets"][0]["cells"][0]["input"]="Region";duplicate["sheets"][0]["cells"][1]["input"]="Region";duplicate["sheets"][0]["cells"].push_back({{"row",2},{"col",1},{"input","East"}});duplicate["sheets"][0]["cells"].push_back({{"row",2},{"col",2},{"input","2"}});
 WorkbookSession duplicateSession(duplicate);WorksheetModel duplicateModel(duplicateSession);QTableView duplicateView;duplicateView.setModel(&duplicateModel);bool rejected=false;try{modelingCommand(nullptr,duplicateSession,duplicateModel,duplicateView,"pivot.create",{{"range","A1:B2"},{"rowField","Region"},{"valueField","Region"},{"aggregate","sum"},{"output","D1"}});}catch(const std::exception&){rejected=true;}require(rejected&&duplicateSession.snapshot()==duplicate,"Duplicate pivot headers must be rejected without mutation");
}

}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 const std::string selected=argc>1?argv[1]:"all";
 if(selected=="context")linkedAnalysisUsesFrozenContext();
 else if(selected=="scenario")scenariosPreserveNewFormulas();
 else {goalSeekAction();linkedAnalysisUsesFrozenContext();solverAction();scenarioActions();scenariosPreserveNewFormulas();pivotAction();sensitivityAction();udfModels();nativeDialogsAreTypedAndCancellable();malformedArguments();failuresAreAtomic();}
 std::cout<<"modeling UI tests passed\n";}catch(const std::exception& e){std::cerr<<"modeling UI test failed: "<<e.what()<<'\n';return 1;}}
