#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
Json book(){return {{"revision",7},{"sheets",Json::array({{{"id","s"},{"name","Sales"},{"cells",Json::array({
 {{"row",1},{"col",1},{"input","Date"}},{{"row",1},{"col",2},{"input","Region"}},{{"row",1},{"col",3},{"input","Amount"}},
 {{"row",2},{"col",1},{"input","45292"}},{{"row",2},{"col",2},{"input","East"}},{{"row",2},{"col",3},{"input","10"}},
 {{"row",3},{"col",1},{"input","45383"}},{{"row",3},{"col",2},{"input","East"}},{{"row",3},{"col",3},{"input","20"}},
 {{"row",4},{"col",1},{"input","45658"}},{{"row",4},{"col",2},{"input","West"}},{{"row",4},{"col",3},{"input","30"}}
 })}},{{"id","r"},{"name","Report's 2026"},{"cells",Json::array()}}})}};}
Json spec(){return {{"id","p"},{"name","Report"},{"source",{{"sheet","s"},{"row1",1},{"col1",1},{"row2",4},{"col2",3}}},
 {"rows",Json::array({{{"field","Date"},{"group","year"}},{{"field","Date"},{"group","quarter"}},{{"field","Date"},{"group","month"}}})},
 {"columns",Json::array({{{"field","Region"}}})},{"filters",Json::array()},
 {"values",Json::array({{{"field","Amount"},{"aggregate","sum"},{"name","Total \"Revenue\""}}})},
 {"output",{{"sheet","r"},{"row1",5},{"col1",3}}}};}
Json installed(Json snapshot,Json definition){auto generated=generatePivot(snapshot,{{"revision",snapshot.at("revision")},{"status","ok"},{"values",Json::array()}},definition);applyPivotTransaction(snapshot,pivotOperations(snapshot,generated)[0]);return snapshot;}
void groupedLookup(){auto snapshot=installed(book(),spec());
 const auto query=Json::array({{{"field","Date (year)"},{"item",{{"type","number"},{"value",2024}}}},{{"field","Date (quarter)"},{"item",{{"type","number"},{"value",2}}}}});
 require(getPivotData(snapshot,"r",5,3,"Total \"Revenue\"",query)==Json({{"type","number"},{"value",20}}),"grouped year and quarter lookup uses numeric date components, not raw serials");
 require(getPivotData(snapshot,"r",5,3,"Amount",Json::array({{{"field","Date (month)"},{"item",{{"type","number"},{"value",1}}}}}))["value"]==40,"month lookup spans years when year is omitted");
 require(getPivotData(snapshot,"r",5,3,"Amount",Json::array({{{"field","Date (year)"},{"item",{{"type","string"},{"value","2024"}}}}}))["value"]=="#REF!","grouped lookup preserves item types");
 snapshot["pivots"][0]["rows"][0]["nativeName"]="Fiscal Years";require(getPivotData(snapshot,"r",5,3,"Amount",Json::array({{{"field","Fiscal Years"},{"item",{{"type","number"},{"value",2024}}}}}))["value"]==30,"imported native grouped field names resolve to their source date components");
 auto collision=book();collision["sheets"][0]["cells"].push_back({{"row",1},{"col",4},{"input","Date (year)"}});auto definition=spec();definition["source"]["col2"]=4;collision=installed(std::move(collision),definition);require(getPivotData(collision,"r",5,3,"Amount",Json::array({{{"field","Date (year)_"},{"item",{{"type","number"},{"value",2024}}}}}))["value"]==30,"generated grouped aliases use the native collision suffix");
}
void pointedFormulas(){auto snapshot=installed(book(),spec());
 const std::string prefix="=GETPIVOTDATA(\"Total \"\"Revenue\"\"\",'Report''s 2026'!$C$5";
 require(pivotReferenceFormula(snapshot,"r",6,6)==prefix+",\"Date (year)\",2024,\"Date (quarter)\",1,\"Date (month)\",1,\"Region\",\"East\")","clicked intersection identifies caption, absolute anchor and grouped row/column items");
 require(pivotReferenceFormula(snapshot,"r",6,8)==prefix+",\"Date (year)\",2024,\"Date (quarter)\",1,\"Date (month)\",1)","row grand total omits column items");
 require(pivotReferenceFormula(snapshot,"r",9,6)==prefix+",\"Region\",\"East\")","column grand total omits row items");
 require(pivotReferenceFormula(snapshot,"r",9,8)==prefix+")","grand total omits all axis items");
 for(const auto address:{std::pair{5,6},std::pair{6,3},std::pair{1,1}})require(pivotReferenceFormula(snapshot,"r",address.first,address.second).empty(),"headers, labels and ordinary cells retain A1 references");
 auto definition=spec();definition["values"].push_back({{"field","Amount"},{"aggregate","count"},{"name","Records"}});snapshot=installed(book(),definition);require(pivotReferenceFormula(snapshot,"r",6,7).starts_with("=GETPIVOTDATA(\"Records\","),"multiple measures identify the clicked value caption");
}
void movedReportReferences(){auto snapshot=installed(book(),spec());snapshot["sheets"].push_back({{"id","r2"},{"name","Moved Report"},{"cells",Json::array()}});
 snapshot["sheets"][0]["cells"].push_back({{"row",8},{"col",1},{"input","=GETPIVOTDATA(\"Amount\",'Report''s 2026'!$C$5)"}});
 snapshot["sheets"][1]["cells"].push_back({{"row",20},{"col",1},{"input","=$F$6"}});
 snapshot["names"]={{"ReportTotal","='Report''s 2026'!$C$5"}};
 auto definition=spec();definition["output"]={{"sheet","r2"},{"row1",10},{"col1",10}};auto generated=generatePivot(snapshot,{{"revision",7},{"status","ok"},{"values",Json::array()}},definition);
 applyPivotTransaction(snapshot,pivotOperations(snapshot,generated)[0]);
 require(snapshot["sheets"][0]["cells"].back()["input"]=="=GETPIVOTDATA(\"Amount\",'Moved Report'!$J$10)","moving a pivot rewrites GETPIVOTDATA's absolute anchor across worksheets");
 require(snapshot["sheets"][1]["cells"][0]["input"]=="='Moved Report'!$M$11","moving a pivot rewrites ordinary dependent references");
 require(snapshot["names"]["ReportTotal"]=="='Moved Report'!$J$10","moving a pivot rewrites defined-name anchors");
 auto unsafe=installed(book(),spec());unsafe["sheets"][0]["cells"].push_back({{"row",8},{"col",1},{"input","=SUM('Report''s 2026'!$F$6:$F$99)"}});const auto before=unsafe;definition=spec();definition["output"]["row1"]=15;generated=generatePivot(unsafe,{{"revision",7},{"status","ok"},{"values",Json::array()}},definition);bool rejected=false;try{applyPivotTransaction(unsafe,pivotOperations(unsafe,generated)[0]);}catch(const std::exception&){rejected=true;}require(rejected&&unsafe==before,"partial intersecting dependent ranges refuse a report move atomically");
 unsafe=installed(book(),spec());unsafe["names"]={{"RelativeReport","=$F$6"}};const auto relativeBefore=unsafe;generated=generatePivot(unsafe,{{"revision",7},{"status","ok"},{"values",Json::array()}},definition);rejected=false;try{applyPivotTransaction(unsafe,pivotOperations(unsafe,generated)[0]);}catch(const std::exception&){rejected=true;}require(rejected&&unsafe==relativeBefore,"ambiguous relative defined names refuse a report move rather than retaining a stale anchor");
}
int main(int argc,char**argv){try{const auto test=argc>1?std::string(argv[1]):"all";if(test=="all"||test=="lookup")groupedLookup();if(test=="all"||test=="pointed")pointedFormulas();if(test=="all"||test=="move")movedReportReferences();std::cout<<"pivot reference tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
