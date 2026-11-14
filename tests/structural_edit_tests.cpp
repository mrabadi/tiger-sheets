#include "tigersheets/session.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include <cstdlib>
#include <iostream>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay){std::cerr<<"FAILED: "<<message<<'\n';std::exit(1);}}
Json cell(int row,int col,const std::string& input){return {{"row",row},{"col",col},{"input",input}};}
Json book(){return {{"revision",0},{"sheets",Json::array({{{"id","data"},{"name","Data"},{"cells",Json::array({cell(1,1,"Header"),cell(2,1,"100"),cell(3,1,"200"),cell(3,2,"=$A$2+A3")})}},{{"id","summary"},{"name","Summary"},{"cells",Json::array({cell(1,1,"=SUM(Data!A2:A3)")})}}})},{"names",{{"Revenue","=Data!$A$2:$A$3"}}}};}
int main(){WorkbookSession session(book());std::string error;
 require(session.apply(0,Json::array({{{"type","insert_rows"},{"sheet","data"},{"index",3},{"count",2}}}),&error),("insert rows succeeds: "+error).c_str());
 require(session.cellAt("data",5,1)&&session.cellAt("data",5,1)->at("input")=="200"&&session.cellAt("data",5,2)->at("input")=="=$A$2+A5","inserting rows moves values and rebases formula references");
 require(session.cellAt("summary",1,1)->at("input")=="=SUM(Data!A2:A5)"&&session.snapshotView().at("names").at("Revenue")=="=Data!$A$2:$A$5","insertion rebases cross-sheet references and defined names");
 require(session.undo()&&session.snapshotView().at("sheets")==book().at("sheets")&&session.snapshotView().at("names")==book().at("names"),"structural edit is one undo transaction");
 require(session.redo()&&session.cellAt("data",5,1),"redo restores the inserted rows");
 WorkbookSession deleted(book());require(deleted.apply(0,Json::array({{{"type","delete_rows"},{"sheet","data"},{"index",2},{"count",1}}}),&error),("delete rows succeeds: "+error).c_str());
 require(deleted.cellAt("data",2,1)->at("input")=="200"&&deleted.cellAt("data",2,2)->at("input")=="=#REF!+A2"&&deleted.cellAt("summary",1,1)->at("input")=="=SUM(Data!A2:A2)","deleting rows removes records, shrinks ranges and invalidates deleted direct references");
 WorkbookSession columns(book());require(columns.apply(0,Json::array({{{"type","insert_columns"},{"sheet","data"},{"index",1},{"count",2}}}),&error),"insert columns succeeds");
 require(columns.cellAt("data",3,4)->at("input")=="=$C$2+C3"&&columns.cellAt("summary",1,1)->at("input")=="=SUM(Data!C2:C3)","column insertion moves typed source cells and rebases cross-sheet formulas");
 require(columns.apply(1,Json::array({{{"type","delete_columns"},{"sheet","data"},{"index",3},{"count",1}}}),&error),"delete columns succeeds");
 require(columns.cellAt("data",3,3)->at("input")=="=#REF!+#REF!"&&columns.cellAt("summary",1,1)->at("input")=="=SUM(Data!#REF!)","column deletion invalidates direct and whole-deleted rectangle references");
 const auto unchanged=[&](Json fixture,const Json& operation,const char* message){WorkbookSession guarded(fixture);require(!guarded.apply(0,Json::array({operation}),&error)&&guarded.snapshot()==fixture&&!guarded.canUndo(),message);};
 auto boundary=book();boundary["sheets"][0]["cells"].push_back(cell(1048576,1,""));boundary["sheets"][0]["cells"].back()["style"]={{"bold",true}};unchanged(boundary,{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}},"row overflow rejects atomically including formatted blank cells");
 for(const auto& invalid:Json::array({Json{{"index",0},{"count",1}},Json{{"index",2},{"count",0}},Json{{"index",2},{"count",1.5}},Json{{"index",1048576},{"count",2}}})){auto op=invalid;op["type"]="insert_rows";op["sheet"]="data";unchanged(book(),op,"invalid structural interval rejects without workbook/history changes");}
 auto named=book();named["names"]["Unqualified"]="=$A$3";unchanged(named,{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}},"unqualified workbook names refuse ambiguous rebasing atomically");
 auto charted=book();charted["charts"]=Json::array({{{"id","c"},{"type","line"},{"sheet","data"},{"range",{{"row1",1},{"row2",3},{"col1",1},{"col2",2}}}}});unchanged(charted,{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}},"chart-dependent worksheet requires source adjustment before structure changes");
 auto unsupported=book();unsupported["sheets"][1]["cells"][0]["input"]="=SUM(Data:Summary!A2)";unchanged(unsupported,{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}},"unsupported formulas cannot leave silently stale references");
 auto printed=book();printed["settings"]={{"printSheets",{{"data",{{"area",{{"row1",1},{"row2",3},{"col1",1},{"col2",2}}}}}}}};unchanged(printed,{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}},"saved print areas must not be left pointing to stale coordinates");
 auto reported=book();reported["sheets"][1]["cells"]=Json::array({cell(1,1,"Region"),cell(1,2,"Amount"),cell(2,1,"East"),cell(2,2,"=Data!A2")});reported["sheets"].push_back({{"id","report"},{"name","Report"},{"cells",Json::array()}});
 const Json definition={{"id","p1"},{"name","Pivot1"},{"source",{{"sheet","summary"},{"row1",1},{"col1",1},{"row2",2},{"col2",2}}},{"rows",Json::array({{{"field","Region"}}})},{"values",Json::array({{{"field","Amount"},{"aggregate","sum"}}})},{"output",{{"sheet","report"},{"row1",1},{"col1",1}}}};
 const auto generated=generatePivot(reported,{{"revision",0},{"status","ok"},{"values",Json::array({{{"sheet","summary"},{"row",2},{"col",2},{"type","number"},{"value",100}}})}},definition,1);applyPivotTransaction(reported,pivotOperations(reported,generated)[0]);WorkbookSession dependent(reported);
 require(dependent.apply(0,Json::array({{{"type","insert_rows"},{"sheet","data"},{"index",2},{"count",1}}}),&error),"unrelated report rectangle permits safe structure changes");
 require(dependent.snapshotView().at("pivots")[0].value("stale",false),"structural reference rewrites mark indirectly dependent report caches stale");
 std::cout<<"structural edit tests passed\n";
}
