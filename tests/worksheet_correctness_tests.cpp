#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Json valueAt(const Json& result,int row,int col,const std::string& sheet="sheet-1"){for(const auto& cell:result.at("values"))if(cell.at("sheet")==sheet&&cell.at("row")==row&&cell.at("col")==col)return cell;throw std::runtime_error("calculated cell missing");}
void typedValues(){
 WorkbookSession session;WorksheetModel model(session);CalculationEngine engine;
 for(const auto& [row,input]:std::vector<std::pair<int,std::string>>{{1,"=\"=1+1\""},{2,"=\"00123\""},{3,"=TRUE()"},{4,"=1/0"},{5,"=\"#DIV/0!\""},{6,"=\"\""}})require(model.setData(model.index(row-1,0),QString::fromStdString(input),Qt::EditRole),"source setup");
 model.setCalculated(engine.calculate(session.snapshot()));
 require(model.pasteRange(0,1,model.copyRange(0,0,5,0),"values"),"typed Paste Values succeeds");
 const auto result=engine.calculate(session.snapshot());
 require(valueAt(result,1,2).at("type")=="text"&&valueAt(result,1,2).at("value")=="=1+1","Paste Values must preserve text beginning equals instead of evaluating it");
 require(valueAt(result,2,2).at("type")=="text"&&valueAt(result,2,2).at("value")=="00123","Paste Values preserves numeric-looking text and leading zeroes");
 require(valueAt(result,3,2).at("type")=="boolean"&&valueAt(result,3,2).at("value")==true,"Paste Values preserves Boolean values");
 require(valueAt(result,4,2).at("type")=="error"&&valueAt(result,4,2).at("value")=="#DIV/0!","Paste Values preserves errors as errors");
 require(valueAt(result,5,2).at("type")=="text"&&valueAt(result,5,2).at("value")=="#DIV/0!","Paste Values distinguishes error-looking text from errors");
 require(valueAt(result,6,2).at("type")=="text"&&valueAt(result,6,2).at("value")=="","Paste Values preserves empty text");
}
void missingValues(){
 WorkbookSession session;WorksheetModel model(session);
 require(model.setData(model.index(0,0),"constant",Qt::EditRole)&&model.setData(model.index(1,0),"=1+2",Qt::EditRole),"uncalculated clipboard setup");
 const auto before=session.snapshot();
 require(!model.pasteRange(0,1,model.copyRange(0,0,1,0),"values"),"Paste Values must refuse an uncached formula instead of pasting a live formula");
 require(session.snapshot()==before,"missing calculated values refuse the whole paste atomically");
 require(model.pasteRange(0,1,model.copyRange(0,0,0,0),"values"),"literal cells can paste without a calculation cache");
}
void sortedFormulas(){
 WorkbookSession session;WorksheetModel model(session);CalculationEngine engine;
 for(const auto& [row,col,input]:std::vector<std::tuple<int,int,const char*>>{{0,0,"=2"},{0,1,"=A1*10"},{1,0,"=1"},{1,1,"=A2*10"}})require(model.setData(model.index(row,col),input,Qt::EditRole),"formula sort setup");
 model.setCalculated(engine.calculate(session.snapshot()));const auto before=session.snapshot();
 require(model.sortRange(0,0,1,1,true),"financial rows sort by calculated keys");const auto result=engine.calculate(session.snapshot());
 require(valueAt(result,1,1).at("value")==1&&valueAt(result,1,2).at("value")==10&&valueAt(result,2,2).at("value")==20,"sorting rows preserves same-row formula associations");
 require(model.raw(0,1)=="=A1*10"&&model.raw(1,1)=="=A2*10","sorted formulas rebase from their original row positions");
 require(session.undo()&&session.snapshot().at("sheets")==before.at("sheets"),"sorting is one undoable transaction");
}
void atomicMove(){
 WorkbookSession session;WorksheetModel model(session);CalculationEngine engine;
 require(model.setData(model.index(0,0),"10",Qt::EditRole)&&model.setData(model.index(0,1),"=A1*2",Qt::EditRole)&&model.setData(model.index(0,2),"=$B$1+1",Qt::EditRole),"cut setup");
 const auto before=session.snapshot();const auto clip=model.cutRange(0,1,0,1);require(session.snapshot()==before,"Cut must leave source intact until successful paste");
 require(model.pasteRange(1,1,clip),"cut paste succeeds");const auto result=engine.calculate(session.snapshot());
 require(model.raw(0,1).isEmpty()&&model.raw(1,1)=="=A1*2"&&model.raw(0,2)=="=$B$2+1","move preserves outside references and updates locked dependent references");
 require(valueAt(result,2,2).at("value")==20&&valueAt(result,1,3).at("value")==21,"move preserves financial model values");
 require(session.undo()&&session.snapshot().at("sheets")==before.at("sheets"),"cut paste is one undoable transaction");
 model.refresh();require(!model.pasteRange(1,1,clip)&&session.snapshot().at("sheets")==before.at("sheets"),"stale cuts cannot remove cells after another transaction");
 WorkbookSession overlap;WorksheetModel om(overlap);require(om.setData(om.index(0,0),"1",Qt::EditRole)&&om.setData(om.index(1,0),"=A1+1",Qt::EditRole)&&om.setData(om.index(0,1),"=SUM(A1:A2)",Qt::EditRole),"overlap setup");
 require(om.pasteRange(1,0,om.cutRange(0,0,1,0)),"overlapping moves preserve source snapshot");
 require(om.raw(0,0).isEmpty()&&om.raw(1,0)=="1"&&om.raw(2,0)=="=A2+1"&&om.raw(0,1)=="=SUM(A2:A3)","overlapping move updates internal and dependent range references");
 WorkbookSession cross;WorksheetModel cm(cross);require(cm.apply(Json::array({{{"type","add_sheet"},{"id","target"},{"name","Target sheet"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","10"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",2},{"input","=A1*2"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",3},{"input","=B1+1"}},{{"type","set_metadata"},{"key","names"},{"value",{{"Moved","Sheet1!$B$1"}}}}})),"cross-sheet setup");
 const auto crossClip=cm.cutRange(0,1,0,1);cm.setSheetId("target");require(cm.pasteRange(1,1,crossClip),"cross-sheet cut paste succeeds");const auto crossResult=engine.calculate(cross.snapshot());
 require(cm.raw(1,1)=="='Sheet1'!A1*2"&&valueAt(crossResult,2,2,"target").at("value")==20&&valueAt(crossResult,1,3).at("value")==21,"cross-sheet move preserves outside references and retargets dependents");
 require(cross.snapshot().at("names").at("Moved")=="'Target sheet'!$B$2","move updates workbook named-range references");
}
void moveGuard(){
 WorkbookSession session;WorksheetModel model(session);require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","10"}},{{"type","set_metadata"},{"key","charts"},{"value",Json::array({{{"id","chart"},{"sheet","sheet-1"},{"type","column"},{"range",{{"row1",1},{"col1",1},{"row2",2},{"col2",2}}},{"anchor",{{"row",4},{"col",1},{"width",400},{"height",250}}}}})}}})),"chart move guard setup");
 const auto before=session.snapshot();require(!model.pasteRange(3,0,model.cutRange(0,0,0,0)),"moving chart source cells must be refused until chart references can move safely");require(session.snapshot()==before&&model.lastError().contains("chart",Qt::CaseInsensitive),"unsafe chart-source move leaves workbook intact and explains refusal");
 require(!model.pasteRange(0,0,model.cutRange(3,0,3,0)),"overwriting chart source through a move is also refused");
 require(model.pasteRange(5,0,model.cutRange(3,0,3,0)),"moves unrelated to chart data remain allowed");
}
void targetedRefresh(){WorkbookSession session;WorksheetModel model(session);require(model.setData(model.index(0,0),"base",Qt::EditRole),"refresh fixture");int notifications=0;QModelIndex first,last;QObject::connect(&model,&QAbstractItemModel::dataChanged,[&](const QModelIndex& a,const QModelIndex& b){++notifications;first=a;last=b;});require(model.setData(model.index(4,2),"new",Qt::EditRole),"targeted model edit");require(notifications==1&&first==model.index(4,2)&&last==model.index(4,2),"an ordinary edit refreshes only its changed worksheet rectangle");require(model.raw(0,0)=="base"&&model.raw(4,2)=="new","targeted refresh reads current canonical cells without replacing unrelated data");}
}
int main(int argc,char**argv){QApplication app(argc,argv);try{const auto test=argc>1?std::string(argv[1]):"all";if(test=="typed"||test=="all")typedValues();if(test=="missing"||test=="all")missingValues();if(test=="sort"||test=="all")sortedFormulas();if(test=="move"||test=="all")atomicMove();if(test=="guard"||test=="all")moveGuard();if(test=="refresh"||test=="all")targetedRefresh();std::cout<<"worksheet correctness "<<test<<" passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
