#include "tigersheets/calculation.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QTest>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
Json fixture(){return Json::parse(R"FORM({"revision":0,"sheets":[{"id":"model","name":"Model","cells":[
 {"row":8,"col":3,"input":"=-'[Source Financials.xlsx]operations'!G12"},
 {"row":9,"col":3,"input":"=-'[Source Financials.xlsx]operations'!G13"},
 {"row":10,"col":3,"input":"=-SUM('[Source Financials.xlsx]operations'!G14:G15)"},
 {"row":10,"col":4,"input":"=-SUM('[Source Financials.xlsx]operations'!H14:H15)"},
 {"row":10,"col":5,"input":"=-SUM('[Source Financials.xlsx]operations'!I14:I15)"},
 {"row":1,"col":11,"input":"1"},{"row":2,"col":11,"input":"2"},{"row":3,"col":11,"input":"3"},
 {"row":4,"col":11,"input":"=SUM(K1:K3)"},
 {"row":5,"col":11,"input":"4"},{"row":6,"col":11,"input":"5"},{"row":7,"col":11,"input":"6"},
 {"row":8,"col":11,"input":"=SUM(K5:K6)"},
 {"row":10,"col":11,"input":"=SUM(K1:K3,K5:K7)"},
 {"row":12,"col":11,"input":"=SUM(L1:L3)"},
 {"row":1,"col":12,"input":"'1"},{"row":2,"col":12,"input":"text"},{"row":3,"col":12,"input":"TRUE"},
 {"row":4,"col":12,"input":"8"}]}],"links":[{"name":"Source Financials.xlsx","enabled":true,"workbook":{"revision":0,"sheets":[{"id":"source","name":"operations","cells":[
 {"row":12,"col":7,"input":"5293"},{"row":13,"col":7,"input":"4812"},{"row":14,"col":7,"input":"159"},{"row":15,"col":7,"input":"1948"},{"row":16,"col":7,"input":"12212"},
 {"row":13,"col":8,"input":"4460"},{"row":14,"col":8,"input":"155"},{"row":15,"col":8,"input":"1810"},{"row":16,"col":8,"input":"11381"},
 {"row":13,"col":9,"input":"4640"},{"row":14,"col":9,"input":"163"},{"row":15,"col":9,"input":"1817"},{"row":16,"col":9,"input":"11265"}]}]}}]})FORM");}
bool flagged(const Json& result,int row,int col){for(const auto& entry:result.value("errorChecks",Json::array()))if(entry.value("sheet",std::string())=="model"&&entry.value("row",0)==row&&entry.value("col",0)==col&&entry.value("code",std::string())=="formulaRange")return true;return false;}
void calculation(){auto book=fixture();const auto before=book;CalculationEngine engine;auto result=engine.calculate(book);require(result.at("status")=="ok","source fixture calculates natively");for(int col=3;col<=5;++col)require(flagged(result,10,col),"SUM of an external numeric subrange produces the source green-warning metadata for omitted adjacent source numbers");require(flagged(result,8,11),"ordinary local SUM flags omitted numeric cells between its range and formula");require(!flagged(result,8,3)&&!flagged(result,9,3)&&!flagged(result,4,11)&&!flagged(result,10,11)&&!flagged(result,12,11),"direct references, complete self-adjacent totals, multiple arguments and nonnumeric blocks remain unflagged");require(book==before,"error checking is transient and never edits the workbook");book["sheets"][0]["ignoredErrors"]=Json::array({{{"ranges",Json::array({{{"row1",10},{"col1",3},{"row2",10},{"col2",3}}})},{"checks",{{"formulaRange",true}}}},{{"ranges",Json::array({{{"row1",10},{"col1",4},{"row2",10},{"col2",4}}})},{"checks",{{"formulaRange",false}}}},{{"ranges",Json::array({{{"row1",10},{"col1",5},{"row2",10},{"col2",5}}})},{"checks",{{"formula",true}}}}});result=engine.calculate(book);require(!flagged(result,10,3)&&flagged(result,10,4)&&flagged(result,10,5),"only true native formulaRange ignore flags suppress this warning");}
void pixels(){WorkbookSession session(fixture());WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.verticalHeader()->setDefaultSectionSize(20);view.horizontalHeader()->setDefaultSectionSize(64);view.resize(900,420);view.show();QApplication::processEvents();const auto before=session.snapshot();auto result=CalculationEngine().calculate(fixture());result["errorChecks"]=Json::array({{{"sheet","model"},{"row",10},{"col",3},{"code","formulaRange"}}});model.setCalculated(result);QApplication::processEvents();const auto cell=view.visualRect(model.index(9,2));const auto direct=view.visualRect(model.index(7,2));const auto image=view.viewport()->grab().toImage();require(image.pixelColor(cell.left()+2,cell.top()+2)==QColor("#008000"),"native worksheet paints the source green triangle in the actual formula cell's upper-left corner");require(image.pixelColor(direct.left()+2,direct.top()+2)!=QColor("#008000"),"ordinary direct references do not receive a green triangle");require(session.snapshot()==before&&!session.canUndo(),"painting warnings creates no styles, cells, edits or history");if(const auto* capture=std::getenv("TIGERSHEETS_ERROR_CHECK_CAPTURE")){model.setCalculated(CalculationEngine().calculate(fixture()));QApplication::processEvents();require(view.grab().save(QString::fromLocal8Bit(capture)),"save actual native error-check capture");}model.refresh();QApplication::processEvents();require(view.viewport()->grab().toImage().pixelColor(cell.left()+2,cell.top()+2)!=QColor("#008000"),"refresh clears stale warning metadata before a new calculation");result["revision"]=9;model.setCalculated(result);QApplication::processEvents();require(view.viewport()->grab().toImage().pixelColor(cell.left()+2,cell.top()+2)!=QColor("#008000"),"foreign calculation revisions cannot publish stale triangles");result["revision"]=0;result["iteration"]={{"converged",false}};model.setCalculated(result);QApplication::processEvents();require(view.viewport()->grab().toImage().pixelColor(cell.left()+2,cell.top()+2)!=QColor("#008000"),"nonconverged calculations cannot publish formula checks");result["iteration"]["converged"]=true;model.setCalculated(result);require(model.setData(model.index(9,2),"=1"),"replace the source warning formula through an ordinary native model edit");QApplication::processEvents();require(view.viewport()->grab().toImage().pixelColor(cell.left()+2,cell.top()+2)!=QColor("#008000"),"ordinary editing immediately clears provisional error checking until recalculation");}
void numberText(){
 auto book=WorkbookSession().snapshot();const auto id=book["sheets"][0]["id"].get<std::string>();
 book["sheets"][0]["cells"]=Json::array();
 const char* inputs[]={"'2021","2021","'FY 21","'00123","=1+1","'-3.5","'1e3","'2021","'2021","'2021"};
 for(int row=0;row<10;++row)book["sheets"][0]["cells"].push_back({{"row",row+1},{"col",2},{"input",inputs[row]}});
 book["sheets"][0]["ignoredErrors"]=Json::array({
  {{"ranges",Json::array({{{"row1",8},{"col1",2},{"row2",8},{"col2",2}}})},{"checks",{{"numberStoredAsText",true}}}},
  {{"ranges",Json::array({{{"row1",9},{"col1",2},{"row2",9},{"col2",2}}})},{"checks",{{"formulaRange",true}}}},
  {{"ranges",Json::array({{{"row1",10},{"col1",2},{"row2",10},{"col2",2}}})},{"checks",{{"numberStoredAsText",false}}}}
 });
 const auto before=book;const auto result=CalculationEngine().calculate(book);
 auto flaggedText=[&](int row){for(const auto& entry:result.at("errorChecks"))if(entry.value("sheet",std::string())==id&&entry.value("row",0)==row&&entry.value("col",0)==2&&entry.value("code",std::string())=="numberStoredAsText")return true;return false;};
 for(const int row:{1,4,6,7,9,10})require(flaggedText(row),"numeric strings, including source-style year headings, produce number-stored-as-text warnings independently of formula-range ignores");
 for(const int row:{2,3,5,8})require(!flaggedText(row),"numeric values, ordinary text, formulas and explicitly ignored numeric text stay unflagged");
 WorkbookSession session(book);WorksheetModel model(session);
 const auto intrinsic=[&]{for(int row=1;row<=10;++row)require(model.data(model.index(row-1,1),FormulaErrorCheckRole).toBool()==flaggedText(row),"intrinsic warnings honor actual input type and only the specific true ignore flag without depending on calculation state");};
 intrinsic();model.setCalculated({{"revision",0},{"status","stale"},{"values",Json::array()}});intrinsic();model.setCalculated({{"revision",0},{"status","ok"},{"iteration",{{"converged",false}}},{"values",Json::array()}});intrinsic();model.setCalculated({{"revision",99},{"status","ok"},{"values",Json::array()}});intrinsic();
 WorksheetView view;view.setModel(&model);view.resize(500,420);view.show();model.setCalculated(result);QApplication::processEvents();const auto image=view.viewport()->grab().toImage();
 const auto year=view.visualRect(model.index(0,1));const auto ordinary=view.visualRect(model.index(1,1));
 require(image.pixelColor(year.left()+2,year.top()+2)==QColor("#008000"),"the actual source-style text year paints its green upper-left warning triangle");
 require(image.pixelColor(ordinary.left()+2,ordinary.top()+2)!=QColor("#008000"),"a numeric year does not receive a text warning triangle");
 require(book==before&&session.snapshot().at("sheets")==before.at("sheets")&&!session.canUndo(),"numeric-text checking and native painting preserve types, styles and workbook history");
 require(model.setData(model.index(0,1),"2021"),"replace a text year with an actual numeric input");require(!model.data(model.index(0,1),FormulaErrorCheckRole).toBool(),"editing away the numeric-text input immediately removes the intrinsic warning");require(model.setData(model.index(1,1),"'2021"),"enter a numeric-text year through the real editable model");require(model.data(model.index(1,1),FormulaErrorCheckRole).toBool(),"a newly entered numeric-text value immediately receives its intrinsic warning");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{if(argc>1&&std::string(argv[1])=="pixels")pixels();else if(argc>1&&std::string(argv[1])=="number_text")numberText();else calculation();std::cout<<"Formula error checking passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
