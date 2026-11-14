#include "tigersheets/workbook_tools.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#include <limits>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void rangePreview(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());const auto before=session.snapshot();Json displayed;
 tools.onPreview=[&](const Json& candidate){displayed=candidate;};
 const auto result=tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({{{"type","set_range"},{"sheet","sheet-1"},{"row",1},{"col",1},{"values",Json::array({Json::array({"Revenue",12000000,true}),Json::array({"Forecast","=B1*1.12",nullptr})})}}})}});
 require(result.value("status",std::string())=="preview","a rectangular financial data range creates a preview");
 require(session.snapshot()==before&&session.hasPreview(),"Codex range editing must remain isolated");
 require(displayed["sheets"][0]["cells"][1]["input"]=="12000000"&&displayed["sheets"][0]["cells"][2]["input"]=="TRUE"&&displayed["sheets"][0]["cells"][4]["input"]=="=B1*1.12"&&displayed["sheets"][0]["cells"][5]["input"]=="","range values retain native input/formula semantics");
}
Json revenueChart(std::string id="forecast"){
 return {{"id",id},{"sheet","sheet-1"},{"type","line"},{"title","Fictitious company revenue forecast"},{"range",{{"row1",1},{"col1",1},{"row2",4},{"col2",2}}},{"anchor",{{"row",6},{"col",1},{"width",560},{"height",300}}}};
}
void composedFinancialPreview(){
 WorkbookSession session;const auto existing=revenueChart("existing");require(session.apply(0,Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({existing})}}})),"existing chart fixture");
 WorkbookTools tools(session,Json::object());const auto before=session.snapshot();Json displayed;tools.onPreview=[&](const Json& book){displayed=book;};
 const Json range={{"type","set_range"},{"sheet","sheet-1"},{"row",1},{"col",1},{"values",Json::array({Json::array({"Year","Revenue"}),Json::array({"2025",12000000}),Json::array({"2026","=B2*1.12"}),Json::array({"2027","=B3*1.10"})})}};
 const auto first=tools.invoke("workbook_v1_preview",{{"revision",1},{"operations",Json::array({range})}});
 require(first.value("status",std::string())=="preview","financial data first preview");
 const auto second=tools.invoke("workbook_v1_preview",{{"revision",1},{"operations",Json::array({{{"type","add_chart"},{"chart",revenueChart()}}})}});
 require(second.value("status",std::string())=="preview","a later chart request extends the financial preview");
 const auto read=tools.invoke("workbook_v1_read",{{"sheet","sheet-1"}});
 require(read.at("revision")==1&&read.at("view")=="preview"&&read.at("previewRevision")==2,"preview reads retain the live base revision for subsequent operations");
 require(read.at("sheets")[0]["cells"].size()==8&&read.at("sheets")[0]["cells"][5]["input"]=="=B2*1.12","later chart operations preserve all proposed financial cells");
 require(read.at("charts").size()==2&&read.at("charts")[0]==existing&&displayed.at("charts").size()==2,"native chart creation preserves existing charts and displays the combined branch");
 require(session.snapshot()==before,"multiple tool calls never commit the preview");
 require(session.accept()&&session.snapshot()["sheets"][0]["cells"].size()==8,"the combined proposal can be accepted");
 require(session.undo()&&session.snapshot()["sheets"]==before["sheets"]&&session.snapshot()["charts"]==before["charts"],"one undo restores the complete workbook before the financial proposal");
}
void discoverableContract(){
 WorkbookSession session;WorkbookTools tools(session,{{"functions",{"SUM","NPV"}}});
 const auto capabilities=tools.invoke("workbook_v1_capabilities",Json::object());
 require(!capabilities.contains("error")&&capabilities.at("version")=="workbook.v1","the built-in workbook contract is discoverable");
 require(capabilities.at("operationSchemas").size()>=8&&capabilities.at("sheets")[0]["id"]=="sheet-1","capabilities publish exact operations and stable worksheet identities");
 require(capabilities.at("examples").at("financialModel").at("operations").size()>=2,"a complete financial range and native chart example is supplied");
 require(capabilities.at("formulas").at("functions")==Json::array({"SUM","NPV"}),"the host's available formula catalog is authoritative");
 const auto definitions=WorkbookTools::definitions();const Json* preview=nullptr;std::set<std::string> names;
 for(const auto& definition:definitions){names.insert(definition.at("name").get<std::string>());if(definition.at("name")=="workbook_v1_preview")preview=&definition.at("inputSchema");}
 for(const auto* name:{"workbook_v1_capabilities","workbook_v1_read","workbook_v1_search","workbook_v1_preview","workbook_v1_report","workbook_v1_reports","workbook_v1_pivot_members"})require(names.contains(name),"original workbook tools and reporting tools remain discoverable by stable name");
 require(preview&&preview->at("properties").at("operations").at("items").at("oneOf").size()>=8,"the actual dynamic preview tool carries full operation schemas");
}
void invalidRangesPreserveProposal(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());int callbacks=0;tools.onPreview=[&](const Json&){++callbacks;};
 const Json good={{"type","set_range"},{"sheet","sheet-1"},{"row",1},{"col",1},{"values",Json::array({Json::array({"Keep",42})})}};
 require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({good})}}).contains("status"),"valid preview fixture");
 const auto before=tools.invoke("workbook_v1_read",Json::object());
 std::vector<Json> bad;
 for(const auto& value:std::vector<Json>{0,-1,1.5,true,std::numeric_limits<std::uint64_t>::max(),"1"})for(const auto* key:{"row","col"}){auto op=good;op[key]=value;bad.push_back(op);}
 for(const auto& values:std::vector<Json>{Json::array(),Json::object(),Json::array({Json::array()}),Json::array({Json::array({1,2}),Json::array({3})}),Json::array({Json::array({Json::object()})}),Json::array({Json::array({std::numeric_limits<double>::infinity()})})}){auto op=good;op["values"]=values;bad.push_back(op);}
 auto overflow=good;overflow["col"]=16384;bad.push_back(overflow);
 auto missingSheet=good;missingSheet["sheet"]="unknown";bad.push_back(missingSheet);
 auto oversized=good;oversized["values"]=Json::array({Json::array()});for(int i=0;i<10000;++i)oversized["values"][0].push_back(i);bad.push_back(oversized);
 for(const auto& op:bad){const auto response=tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({op})}});require(response.contains("error"),"invalid matrix types, dimensions, coordinates and aggregate limits must reject");require(tools.invoke("workbook_v1_read",Json::object())==before&&callbacks==1,"invalid requests preserve the existing valid proposal without another display callback");}
 require(tools.invoke("workbook_v1_preview",{{"revision",1},{"operations",Json::array({good})}}).contains("error"),"stale revision requests reject without replacing the proposal");
 require(tools.invoke("workbook_v1_read",Json::object())==before&&session.snapshot()["sheets"][0]["cells"].empty(),"invalid and stale requests preserve canonical cells and proposal state");
}
void chartValidation(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());const auto chart=revenueChart();
 require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({{{"type","add_chart"},{"chart",chart}}})}}).contains("status"),"valid native chart preview");
 const auto before=tools.invoke("workbook_v1_read",Json::object());std::vector<Json> invalid;
 auto duplicate=chart;invalid.push_back(duplicate);
 for(const auto* key:{"id","sheet","type","title","range"}){auto value=chart;value["id"]="second";value.erase(key);invalid.push_back(value);}
 for(const auto& [key,value]:std::vector<std::pair<std::string,Json>>{{"id",""},{"sheet","unknown"},{"type","unimplemented"}}){auto changed=chart;changed["id"]="second";changed[key]=value;invalid.push_back(changed);}
 for(const auto& [key,value]:std::vector<std::pair<std::string,Json>>{{"row1",1.5},{"row2",1},{"col2",1},{"row2",10002},{"col2",102}}){auto changed=chart;changed["id"]="second";changed["range"][key]=value;invalid.push_back(changed);}
 for(const auto& [key,value]:std::vector<std::pair<std::string,Json>>{{"row",0},{"col",16385},{"width",false},{"height",100},{"offsetX",-1}}){auto changed=chart;changed["id"]="second";changed["anchor"][key]=value;invalid.push_back(changed);}
 for(const auto& malformed:invalid){require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({{{"type","add_chart"},{"chart",malformed}}})}}).contains("error"),"new charts require unique ids, native families, real sheets, rectangular data and bounded geometry");require(tools.invoke("workbook_v1_read",Json::object())==before,"invalid chart requests preserve the prior proposal");}
 tools.reset();session.discard();auto defaults=chart;defaults.erase("anchor");require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({{{"type","add_chart"},{"chart",defaults}}})}}).contains("status"),"omitted chart anchor uses explicit native defaults");
 require(tools.invoke("workbook_v1_read",Json::object())["charts"][0]["anchor"].contains("width"),"materialized anchor geometry survives the renderer/persistence boundary");
}
void nativeOperationsAndMacroPermission(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());
 const Json good={{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","Keep"}};
 require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({good})}}).contains("status"),"native cell preview fixture");
 const auto before=tools.invoke("workbook_v1_read",Json::object());
 std::vector<Json> bad;
 auto fractional=good;fractional["row"]=1.5;bad.push_back(fractional);
 auto unsignedCell=good;unsignedCell["col"]=std::numeric_limits<std::uint64_t>::max();bad.push_back(unsignedCell);
 auto unknown=good;unknown["type"]="replace_sheet_cells";unknown["cells"]=Json::array();bad.push_back(unknown);
 auto extra=good;extra["arbitrary"]=true;bad.push_back(extra);
 bad.push_back({{"type","clear_range"},{"sheet","sheet-1"},{"row1",1},{"col1",1},{"row2",10000},{"col2",2}});
 bad.push_back({{"type","set_metadata"},{"key","vba"},{"value",{{"enabled",true},{"modules",Json::array()}}}});
 bad.push_back({{"type","set_metadata"},{"key","charts"},{"value",Json::array({revenueChart(),revenueChart()})}});
 auto incomplete=revenueChart();incomplete.erase("range");bad.push_back({{"type","set_metadata"},{"key","charts"},{"value",Json::array({incomplete})}});
 for(const auto& operation:bad){require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({operation})}}).contains("error"),"native tool validation rejects narrowing, unadvertised operations, oversized edits, duplicate charts and macro enablement");require(tools.invoke("workbook_v1_read",Json::object())==before,"invalid native mutations preserve the prior proposal");}
 session.discard();tools.reset();require(session.apply(0,Json::array({{{"type","set_metadata"},{"key","vba"},{"value",{{"enabled",true},{"projectPresent",true},{"modules",Json::array()}}}}})),"user-enabled macro fixture");
 require(tools.invoke("workbook_v1_preview",{{"revision",1},{"operations",Json::array({{{"type","set_metadata"},{"key","vba"},{"value",{{"modules",Json::array({{{"name","Forecast"},{"source","Sub Forecast()\nEnd Sub"}}})}}}}})}}).contains("status"),"source proposals can preserve an already user-enabled project");
 require(session.accept()&&session.snapshot()["vba"]["enabled"]==true&&session.snapshot()["vba"]["projectPresent"]==true,"source editing preserves the user's existing enablement and original project metadata");
}
void boundedReadsAndSearch(){
 WorkbookSession session;const Json names={{"Revenue","Sheet1!$B$2"}};require(session.apply(0,Json::array({{{"type","set_metadata"},{"key","names"},{"value",names}}})),"named financial reference fixture");WorkbookTools tools(session,Json::object());
 require(tools.invoke("workbook_v1_preview",{{"revision",1},{"operations",Json::array({{{"type","set_range"},{"sheet","sheet-1"},{"row",1},{"col",1},{"values",Json::array({Json::array({"Year","Revenue"}),Json::array({"2026F","=12000000*1.12"})})}}})}}).contains("status"),"financial read fixture");
 const auto search=tools.invoke("workbook_v1_search",{{"query","revenue"}});
 require(search.at("view")=="preview"&&search.at("revision")==1&&search.at("matches").size()==1&&search.at("matches")[0]["cell"]["input"]=="Revenue","search reads case-insensitive pending financial data at the live base revision");
 const auto read=tools.invoke("workbook_v1_read",{{"sheet","sheet-1"},{"row1",2},{"row2",2},{"col1",2},{"col2",2}});
 require(read.at("sheets")[0]["cells"].size()==1&&read.at("metadata").at("names")==names,"bounded reads include the metadata required for safe replacement");
 require(tools.invoke("workbook_v1_read",{{"sheet","unknown"}}).contains("error")&&tools.invoke("workbook_v1_read",{{"row1",1.5}}).contains("error")&&tools.invoke("workbook_v1_read",{{"row1",2},{"row2",1}}).contains("error"),"invalid bounded reads fail explicitly");
 session.discard();tools.reset();auto oversized=Json{{"LongReference",std::string(262145,'A')}};require(session.apply(1,Json::array({{{"type","set_metadata"},{"key","names"},{"value",oversized}}})),"oversized preserved metadata fixture");
 const auto large=tools.invoke("workbook_v1_read",Json::object());require(!large.at("metadata").contains("names")&&large.at("metadataTruncated")==Json::array({"names"}),"oversized metadata is explicitly omitted rather than silently truncated");
 require(tools.invoke("workbook_v1_preview",{{"revision",2},{"operations",Json::array({{{"type","set_metadata"},{"key","names"},{"value",Json::object()}}})}}).contains("error"),"unreadable metadata cannot be destructively replaced by Codex");
}
void calculatedPreviewRead(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());int calculations=0;
 tools.calculate=[&](const Json& book){++calculations;require(book["sheets"][0]["cells"][0]["input"]=="=12000000*1.12","calculation inspects the pending formula");return Json{{"revision",book.at("revision")},{"status","ok"},{"values",Json::array({{{"sheet","sheet-1"},{"row",2},{"col",2},{"value",13440000}},{{"sheet","sheet-1"},{"row",9},{"col",9},{"value",999}}})}};};
 require(tools.invoke("workbook_v1_preview",{{"revision",0},{"operations",Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",2},{"input","=12000000*1.12"}}})}}).contains("status"),"calculated formula proposal");
 const auto read=tools.invoke("workbook_v1_read",{{"row1",2},{"row2",2},{"col1",2},{"col2",2}});
 require(calculations==1&&read.at("sheets")[0]["cells"].size()==1&&read.at("sheets")[0]["cells"][0]["value"]==13440000&&read.at("sheets")[0]["cells"][0]["input"]=="=12000000*1.12","bounded read merges calculated results into their native formula cells");
 tools.calculate=[](const Json& book){return Json{{"revision",book.at("revision")},{"status","error"},{"diagnostics",{"unsupported formula"}}};};
 const auto failed=tools.invoke("workbook_v1_read",Json::object());require(failed.at("calculation").at("status")=="error"&&failed.at("calculation").at("diagnostics")[0]=="unsupported formula","failed calculations remain visible without replacing raw workbook cells");
}
void acceptedAndDiscardedHistory(){
 WorkbookSession session;WorkbookTools tools(session,Json::object());
 const auto proposal=[&](int col,const char* value){return tools.invoke("workbook_v1_preview",{{"revision",session.snapshot().at("revision")},{"operations",Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",col},{"input",value}}})}});};
 require(proposal(1,"accepted").contains("status")&&session.accept(),"first proposal accepted");
 require(proposal(2,"discarded").contains("status"),"second proposal starts from accepted state");session.discard();
 require(proposal(3,"current").contains("status"),"discarded operations do not reappear in later proposals");
 const auto read=tools.invoke("workbook_v1_read",Json::object());require(read.at("sheets")[0]["cells"].size()==2&&read.at("sheets")[0]["cells"][1]["col"]==3,"accept/discard clear pending operation history");
 require(session.apply(1,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","external edit"}}})),"concurrent live transaction fixture");
 require(tools.invoke("workbook_v1_read",Json::object()).contains("error"),"concurrent changes invalidate the pending base with a visible conflict");
 require(proposal(4,"rebased").contains("status"),"a fresh proposal can use the current live revision after conflict");
 const auto latest=tools.invoke("workbook_v1_read",Json::object());require(latest.at("sheets")[0]["cells"].size()==3&&latest.at("sheets")[0]["cells"][1]["col"]==4,"conflicting proposal operations never leak into a fresh branch");
}
}
int main(){try{rangePreview();composedFinancialPreview();discoverableContract();invalidRangesPreserveProposal();chartValidation();nativeOperationsAndMacroPermission();boundedReadsAndSearch();calculatedPreviewRead();acceptedAndDiscardedHistory();std::cout<<"workbook tools tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
