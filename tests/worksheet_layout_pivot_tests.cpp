#include "tigersheets/calculation.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include "tigersheets/session.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 Json book={{"revision",0},{"sheets",Json::array({
 {{"id","inputs"},{"name","Inputs"},{"cells",Json::array({{{"row",1},{"col",1},{"input","10"}},{{"row",2},{"col",1},{"input","20"}}})}},
 {{"id","source"},{"name","Source"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Period"}},{{"row",1},{"col",2},{"input","Revenue"}},{{"row",2},{"col",1},{"input","All"}},{{"row",2},{"col",2},{"input","=SUBTOTAL(109,Inputs!A1:A2)"}}})}},
 {{"id","report"},{"name","Report"},{"cells",Json::array()}}
 })}};
 const Json spec={{"id","pivot"},{"name","RevenueReport"},{"source",{{"sheet","source"},{"row1",1},{"col1",1},{"row2",2},{"col2",2}}},{"rows",Json::array({{{"field","Period"}}})},{"values",Json::array({{{"field","Revenue"},{"aggregate","sum"}}})},{"output",{{"sheet","report"},{"row1",1},{"col1",1}}}};
 CalculationEngine engine;const auto generated=generatePivot(book,engine.calculate(book),spec,1);WorkbookSession session(book);require(session.apply(0,pivotOperations(book,generated)),"native report created");require(session.snapshotView().at("pivots")[0].at("stale")==false,"fresh report cache");
 std::string error;require(session.apply(1,Json::array({{{"type","set_sheet_layout"},{"sheet","inputs"},{"rows",Json::array({{{"index",2},{"size",24.0}}})}}}),&error),error.c_str());require(session.snapshotView().at("pivots")[0].at("stale")==false,"height-only layout does not stale an indirect report");
 require(session.apply(2,Json::array({{{"type","set_sheet_layout"},{"sheet","inputs"},{"rows",Json::array({{{"index",2},{"size",24.0},{"hidden",true}}})}}}),&error),error.c_str());require(session.snapshotView().at("pivots")[0].at("stale")==true,"hiding an indirect SUBTOTAL input marks every native pivot stale");
 auto valueOf=[](const Json& calculated){for(const auto& cell:calculated.at("values"))if(cell.at("sheet")=="source"&&cell.at("row")==2&&cell.at("col")==2)return cell.at("value").get<double>();throw std::runtime_error("Calculated subtotal missing");};require(valueOf(engine.calculate(session.snapshotView()))==10.0,"hidden input changes indirect source subtotal");require(session.undo(&error),error.c_str());require(session.snapshotView().at("pivots")[0].at("stale")==false&&valueOf(engine.calculate(session.snapshotView()))==30.0,"undo restores row visibility and fresh report state together");require(session.redo(&error),error.c_str());require(session.snapshotView().at("pivots")[0].at("stale")==true&&valueOf(engine.calculate(session.snapshotView()))==10.0,"redo restores indirect report invalidation");
 std::cout<<"Worksheet layout pivot tests passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
