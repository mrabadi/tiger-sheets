#include "tigersheets/main_window.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QDir>
#include <QTableView>
#include <QTemporaryDir>
#include <QHeaderView>
#include <QScrollBar>
#include <fstream>
#include <iostream>
#include <cmath>
#include <map>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const std::string& message){if(!okay)throw std::runtime_error(message);}
Json withoutNativeIdentity(Json style){style.erase("ooxmlStyleId");return style;}
std::map<std::pair<int,int>,Json> values(const Json& calculation){std::map<std::pair<int,int>,Json> result;require(calculation.value("status",std::string())=="ok","native calculation succeeds");for(const auto& value:calculation.at("values")){require(value.value("type",std::string())!="error","no formula error at "+columnName(value.at("col"))+std::to_string(value.at("row").get<int>()));result[{value.at("row"),value.at("col")}]=value.at("value");}return result;}
}
// Reference reconstruction and validation, deliberately not named a video replay:
// the transcript does not expose every silent gesture or custom macro binding.
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state,temporaryOutput;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 const std::string root=TIGERSHEETS_SOURCE_DIR;
 const auto reference=readWorkbook(root+"/docs/validation/otis-90-minute/reference/101-01-90-Minute-Case-Study-Otis-Complete.xlsx");const auto& source=reference.workbook.at("sheets")[0];
 Json independent;std::ifstream(root+"/docs/validation/otis-90-minute/reference/completed-workbook.snapshot.json")>>independent;
 const QString output=qEnvironmentVariable("TIGERSHEETS_OTIS_OUTPUT_DIR",temporaryOutput.path());require(QDir().mkpath(output),"create output directory");
 MainWindow window;window.resize(1440,960);window.show();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());const auto sheet=model->sheetId().toStdString();
 Json layout={{"type","set_sheet_layout"},{"sheet",sheet},{"rows",source.value("rowLayout",Json::array())},{"columns",source.value("columnLayout",Json::array())},{"defaultStyle",withoutNativeIdentity(source.at("defaultStyle"))},{"defaultRowHeight",source.at("defaultRowHeight")},{"showGridLines",source.at("showGridLines")}};
 require(model->apply(Json::array({layout})),model->lastError().toStdString());
 Json stages=Json::array();
 for(const auto& stage:std::vector<std::tuple<std::string,int,int>>{{"Historical statements and forecast cells",52,126},{"Drivers and assumptions",2,51}}){
  Json operations=Json::array();for(const auto& cell:source.at("cells")){const int row=cell.at("row");if(row<std::get<1>(stage)||row>std::get<2>(stage))continue;Json operation={{"type","set_cell"},{"sheet",sheet},{"row",row},{"col",cell.at("col")},{"input",cell.at("input")}};if(cell.contains("style"))operation["style"]=withoutNativeIdentity(cell.at("style"));operations.push_back(std::move(operation));}
  require(model->apply(operations),model->lastError().toStdString());stages.push_back({{"stage",std::get<0>(stage)},{"cells",operations.size()},{"revision",window.currentSession().snapshotView().at("revision")}});
 }
 CalculationEngine engine;const auto calculated=engine.calculate(window.currentSession().snapshot());const auto referenceValues=values(calculated);int checked=0;
 for(const auto& cell:independent.at("sheets")[0].at("cells"))if(cell.value("data_type",std::string())=="f"&&cell.contains("raw_value")){
  const auto found=referenceValues.find({cell.at("row"),cell.at("column")});require(found!=referenceValues.end()&&found->second.is_number(),"formula has numeric result at "+cell.at("address").get<std::string>());const double expected=std::stod(cell.at("raw_value").get<std::string>()),actual=found->second.get<double>();require(std::abs(actual-expected)<=1e-9*std::max(1.0,std::abs(expected)),"native result matches independent published cache at "+cell.at("address").get<std::string>());++checked;
 }
 require(checked==515,"all 515 published formulas are recalculated and compared");
 // At 1:21:43 the video shows C110:J110 as FCF / parent net income,
 // replacing the downloadable file's earlier unlabeled three-year FCF check.
 Json videoFinal=Json::array();for(int col=3;col<=10;++col){const auto name=columnName(col);videoFinal.push_back({{"type","set_cell"},{"sheet",sheet},{"row",110},{"col",col},{"input","=("+name+"109+"+name+"114)/"+name+"72"},{"style",{{"numberFormat","0%"},{"bold",false},{"color","#000000"}}}});}
 require(model->apply(videoFinal),"apply observed final FCF conversion check");window.recalculateNow();const auto finalCalculation=engine.calculate(window.currentSession().snapshot());const auto finalValues=values(finalCalculation);
 for(int col=6;col<=10;++col)require(std::abs(finalValues.at({100,col}).get<double>())<1e-8,"all forecast balance sheets balance");
 const int expectedConversion[]={113,124,133,150,134,133,132,130};for(int col=3;col<=10;++col)require(qRound(finalValues.at({110,col}).get<double>()*100)==expectedConversion[col-3],"video final FCF conversion percentage matches");
 const auto path=output+"/otis-three-statement-replica.xlsx";require(window.saveWorkbook(path),window.lastError().toStdString());const auto reopened=readWorkbook(path.toStdString());const auto reopenedValues=values(engine.calculate(reopened.workbook));require(reopenedValues==finalValues,"every calculated value survives native save/reopen");
 const auto& actual=reopened.workbook.at("sheets")[0];require(actual.at("cells").size()==source.at("cells").size(),"all 777 stored cells including styled blanks survive reconstruction");
 const auto authored=window.currentSession().snapshot();for(const auto& cell:authored.at("sheets")[0].at("cells")){
  const auto found=std::find_if(actual.at("cells").begin(),actual.at("cells").end(),[&](const Json& item){return item.at("row")==cell.at("row")&&item.at("col")==cell.at("col");});require(found!=actual.at("cells").end()&&found->at("input")==cell.at("input"),"every authored cell input survives save/reopen");
  const auto expectedStyle=cell.value("style",Json::object());const auto savedStyle=found->value("style",Json::object());
  for(const auto& [key,value]:expectedStyle.items()){const auto saved=savedStyle.value(key,value.is_boolean()?Json(false):Json());require(saved==value,"every authored cell style survives save/reopen at "+columnName(cell.at("col"))+std::to_string(cell.at("row").get<int>())+": "+key);}
 }
 for(const auto* key:{"defaultRowHeight","showGridLines","rowLayout","columnLayout"})if(authored.at("sheets")[0].contains(key))require(actual.value(key,Json::array())==authored.at("sheets")[0].at(key),std::string("authored layout survives save/reopen: ")+key);
 require(!grid->showGrid()&&grid->rowHeight(300)==qRound(15.75*grid->logicalDpiY()/72.0),"native worksheet renders imported view defaults");
 grid->setCurrentIndex(model->index(1,1));grid->verticalScrollBar()->setValue(0);grid->horizontalScrollBar()->setValue(0);QApplication::processEvents();require(window.grab().save(output+"/native-drivers.png"),"save native rendered driver preview");
 for(const auto& capture:std::vector<std::pair<int,QString>>{{52,"native-income-statement.png"},{74,"native-balance-sheet.png"},{102,"native-cash-flow.png"}}){grid->setCurrentIndex(model->index(capture.first-1,1));grid->scrollTo(model->index(capture.first-1,1),QAbstractItemView::PositionAtTop);QApplication::processEvents();require(window.grab().save(output+"/"+capture.second),"save native rendered financial-statement preview");}
 Json report={{"sourceVideo","https://www.youtube.com/watch?v=O2uPNfErmJo"},{"method","Reconstructed through TigerSheets model transactions from the published workbook, then applied the final visually observed FCF conversion check. This is not an exhaustive video gesture replay."},{"publishedFormulasRecalculated",checked},{"finalStoredCells",actual.at("cells").size()},{"allNativeSaveReopenValuesMatch",true},{"videoFinalConversionPercent",expectedConversion},{"stages",stages},{"calculated",finalCalculation},{"remaining",Json::array({"Every silent video gesture, source XLS editing sequence, custom macros, ribbon icon identity and banner parity are not verified.","Historical inputs are published hardcoded values; the video retains external links to its rearranged raw XLS workbook.","Source UI is Windows Excel; this is native Qt on Linux and pixel identity is not established."})}};
 std::ofstream(output.toStdString()+"/replica-verification.json")<<report.dump(2)<<'\n';std::cout<<"Reconstructed 777 cells; 515 source formulas match independent Excel caches; observed final percentages and save/reopen passed\n"<<path.toStdString()<<'\n';
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
