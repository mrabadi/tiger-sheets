#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/selection_statistics.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void sparseNumericValues(){
 Json cells=Json::array();const std::vector<std::string> inputs={"42","'00123","TRUE","#DIV/0!","","25%","=2+3","=SEQUENCE(2,1,6)","'", "'=1+2"};for(std::size_t row=0;row<inputs.size();++row)cells.push_back({{"row",row+1},{"col",1},{"input",inputs[row]}});
 // Keep the spill destination free; quoted empty/formula-looking text live elsewhere.
 cells[8]["col"]=2;cells[9]["col"]=2;
 WorkbookSession session({{"revision",4},{"sheets",Json::array({{{"id","s"},{"name","Data"},{"cells",cells}}})}});WorksheetModel model(session);auto book=session.snapshot();book["_tigersheetsComputedResults"]=true;CalculationEngine engine;const auto result=engine.calculate(book);require(result.at("status")=="ok","sparse numerical fixture calculates");model.setCalculated(result);
 require(model.numericValue(0,0)==42&&model.numericValue(5,0)==.25&&model.numericValue(6,0)==5&&model.numericValue(7,0)==6&&model.numericValue(8,0)==7,"selection values merge sparse numeric inputs, parsed literals, formulas and spills");
 for(const auto row:{1,2,3,4})require(!model.numericValue(row,0),"selection totals exclude quoted numeric text, booleans, errors and blanks");require(!model.numericValue(8,1)&&!model.numericValue(9,1),"selection totals exclude quoted empty and formula-looking text");
 model.setData(model.index(6,0),"=8+9",Qt::EditRole);require(!model.numericValue(6,0)&&model.numericValue(0,0)==42,"stale formula values are omitted while literal numeric input remains available");model.setCalculated({{"revision",session.snapshotView().at("revision")},{"status","not_converged"},{"values",Json::array()}});require(!model.numericValue(7,0),"failed calculation removes prior numeric spill selection values");
}
void largeSparseSelectionsYield(){
 Json cells=Json::array();for(int row=1;row<=20000;++row)cells.push_back({{"row",row},{"col",1},{"input",row==100?"=2+3":"1"}});WorkbookSession session({{"revision",1},{"sheets",Json::array({{{"id","s"},{"name","Data"},{"cells",std::move(cells)}}})}});WorksheetModel model(session);model.setCalculated({{"revision",1},{"status","ok"},{"values",Json::array({{{"sheet","s"},{"row",100},{"col",1},{"type","number"},{"value",5}},{{"sheet","s"},{"row",1000000},{"col",100},{"type","number"},{"value",7}}})}});
 SelectionStatistics statistics;bool completed=false,ticked=false;SelectionTotals totals;QTimer::singleShot(0,[&]{ticked=true;});statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[]{return true;},[&](SelectionTotals value){totals=value;completed=true;});require(!completed,"whole-worksheet status yields before scanning populated source cells");require(QTest::qWaitFor([&]{return completed;},3000),"whole-worksheet status visits populated cells instead of the enormous blank rectangle");require(ticked&&totals.count==20001&&totals.sum==20011,"cooperative selection totals combine numeric source, formula overrides and distant spill exactly once");
}
void changedJobsNeverPublish(){
 WorkbookSession session;WorksheetModel model(session);model.setData(model.index(0,0),"1",Qt::EditRole);SelectionStatistics statistics;int delivered=0;auto complete=[&](SelectionTotals){++delivered;};auto drain=[](){QApplication::processEvents();QApplication::sendPostedEvents();QApplication::processEvents();};
 statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[]{return true;},complete);model.setData(model.index(0,0),"2",Qt::EditRole);drain();require(delivered==0,"model edits cancel pending totals before borrowed cell iterators can be used");
 bool current=true;statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[&]{return current;},complete);current=false;drain();require(delivered==0,"caller selection/context predicate cancels pending totals");
 statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[]{return true;},complete);statistics.cancel();drain();require(delivered==0,"explicit cancellation never publishes pending totals");
 statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[]{return true;},complete);SelectionTotals latest;statistics.start(model,0,0,0,0,[]{return true;},[&](SelectionTotals totals){latest=totals;++delivered;});drain();require(delivered==1&&latest.count==1&&latest.sum==2,"replacement jobs publish only the current small selection");
 auto temporary=std::make_unique<WorksheetModel>(session);statistics.start(*temporary,0,0,MaxRows-1,MaxColumns-1,[]{return true;},complete);temporary.reset();drain();require(delivered==1,"destroying the worksheet model cancels borrowed selection jobs");
 statistics.start(model,0,0,MaxRows-1,MaxColumns-1,[]{return true;},complete);require(session.apply(session.snapshotView().at("revision"),Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","3"}}})),"external session mutation fixture applies");drain();require(delivered==1,"revision checks cancel external session mutations before using old source storage");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{sparseNumericValues();largeSparseSelectionsYield();changedJobsNeverPublish();std::cout<<"selection statistics tests passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
