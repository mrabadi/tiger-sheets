#include "tigersheets/session.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void chartPixelOffsetsAreValidatedAtomically(){
 const Json chart={{"id","offset-chart"},{"type","line"},{"sheet","sheet-1"},{"range",{{"row1",1},{"col1",1},{"row2",3},{"col2",2}}},{"anchor",{{"row",2},{"col",4},{"width",500},{"height",300},{"offsetX",0},{"offsetY",1000000}}}};
 WorkbookSession valid;require(valid.apply(0,Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({chart})}}})),"zero and maximum chart pixel offsets remain valid");
 const std::vector<Json> invalid={-1,1000001,1.5,"12",true,nullptr,std::numeric_limits<std::uint64_t>::max()};
 for(const auto* field:{"offsetX","offsetY"})for(const auto& value:invalid){
  WorkbookSession session;const auto before=session.snapshot();std::string error;auto malformed=chart;malformed["anchor"][field]=value;
  const Json operations={{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","must roll back"}},{{"type","set_metadata"},{"key","charts"},{"value",Json::array({malformed})}}};
  require(!session.apply(0,operations,&error),"malformed chart pixel offsets must reject the entire transaction");
  require(session.snapshot()==before&&!session.canUndo()&&error.find(field)!=std::string::npos,"malformed chart offsets preserve cells revision and undo with a field diagnostic");
  require(session.preview(0,operations,&error).is_null()&&!session.hasPreview(),"malformed chart offset previews cannot be accepted");
  auto imported=before;imported["charts"]=Json::array({malformed});bool rejected=false;try{WorkbookSession loaded(imported);}catch(const std::exception&){rejected=true;}require(rejected,"imported chart pixel offsets use the same validation");
 }
}
void malformedChartMetadataIsRejectedAtomically(){
 WorkbookSession session;const auto before=session.snapshot();std::string error;
 const Json chart={{"id","chart-1"},{"type","line"},{"sheet","sheet-1"},{"range",{{"row1",1},{"col1",1},{"row2",3},{"col2",2}}},{"anchor",{{"row","bad"},{"col",4},{"width",500},{"height",300}}}};
 const Json operations={{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","must roll back"}},{{"type","set_metadata"},{"key","charts"},{"value",Json::array({chart})}}};
 require(!session.apply(0,operations,&error),"malformed chart anchor must be rejected before commit");
 require(session.snapshot()==before&&!session.canUndo(),"rejected chart batch must preserve cells revision and undo history");
 require(!error.empty(),"rejected chart metadata needs a diagnostic");
 require(session.preview(0,operations,&error).is_null()&&!session.hasPreview(),"malformed chart preview must not become acceptable");
}
void chartMetadataBoundariesAreValidated(){
 const Json valid={{"id","chart-1"},{"type","line"},{"sheet","sheet-1"},{"title","Forecast"},{"range",{{"row1",1},{"col1",1},{"row2",3},{"col2",2}}},{"anchor",{{"row",2},{"col",4},{"width",500},{"height",300}}}};
 std::vector<Json> invalid={Json::object(),Json::array({nullptr}),Json::array({"chart"})};
 for(const auto* key:{"id","type","sheet","title"}){auto chart=valid;chart[key]=17;invalid.push_back(Json::array({chart}));}
 for(const auto* key:{"range","anchor"}){auto chart=valid;chart[key]=false;invalid.push_back(Json::array({chart}));}
 const std::vector<std::pair<std::string,Json>> badRange={{"row1",0},{"row1",1.5},{"row2",1048577},{"row2",0},{"row2",10002},{"col1",16385},{"col2",0},{"col2",102},{"row1",std::numeric_limits<std::uint64_t>::max()}};
 for(const auto& [key,value]:badRange){auto chart=valid;chart["range"][key]=value;invalid.push_back(Json::array({chart}));}
 auto missingRange=valid;missingRange["range"].erase("row1");invalid.push_back(Json::array({missingRange}));
 const std::vector<std::pair<std::string,Json>> badAnchor={{"row","bad"},{"row",0},{"col",16385},{"col",1.5},{"width",0},{"height",8193},{"width",std::numeric_limits<std::uint64_t>::max()}};
 for(const auto& [key,value]:badAnchor){auto chart=valid;chart["anchor"][key]=value;invalid.push_back(Json::array({chart}));}
 for(const auto& charts:invalid){
  WorkbookSession session;const auto before=session.snapshot();std::string error;
  require(!session.apply(0,Json::array({{{"type","set_metadata"},{"key","charts"},{"value",charts}}}),&error),"malformed chart metadata must be rejected");
  require(session.snapshot()==before&&!session.canUndo(),"invalid chart metadata must preserve revision and undo");
  auto loaded=before;loaded["charts"]=charts;bool rejected=false;try{WorkbookSession imported(loaded);}catch(const std::exception&){rejected=true;}require(rejected,"constructing a session must validate imported chart metadata");
 }
 WorkbookSession boundary;auto chart=valid;chart["range"]={{"row1",1},{"row2",10001},{"col1",1},{"col2",101}};chart["anchor"]={{"row",1048576},{"col",16384},{"width",8192},{"height",1}};
 require(boundary.apply(0,Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({chart})}}})),"chartData maximum data rows and series must remain valid");
}
void malformedVbaMetadataIsRejectedAtomically(){
 const std::vector<Json> invalid={nullptr,Json::array(),false,Json{{"enabled","yes"}},Json{{"projectPresent",17}},Json{{"sourceAvailable",Json::array()}},Json{{"modules",Json::object()}},Json{{"modules",Json::array({17})}},Json{{"modules",Json::array({{{"name",17},{"source","Function F()\nEnd Function"}}})}},Json{{"modules",Json::array({{{"name","Module1"},{"source",false}}})}}};
 for(const auto& vba:invalid){
  WorkbookSession session;const auto before=session.snapshot();std::string error;
  const Json operations={{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","must roll back"}},{{"type","set_metadata"},{"key","vba"},{"value",vba}}};
  require(!session.apply(0,operations,&error),"malformed VBA metadata must be rejected before commit");
  require(session.snapshot()==before&&!session.canUndo()&&!error.empty(),"rejected VBA transaction must remain atomic and diagnostic");
  require(session.preview(0,operations,&error).is_null()&&!session.hasPreview(),"malformed VBA preview must not become acceptable");
 }
 WorkbookSession valid;require(valid.apply(0,Json::array({{{"type","set_metadata"},{"key","vba"},{"value",{{"enabled",false},{"projectPresent",true},{"sourceAvailable",false},{"modules",Json::array()}}}}})),"valid imported opaque VBA metadata must remain supported");
}
void malformedCellStylesAreRejectedAtomically(){
 std::vector<Json> invalid={Json{{"fontSize","bad"}},Json{{"fontSize",false}},Json{{"fontSize",0}},Json{{"fontSize",401}},Json{{"fontSize",std::numeric_limits<double>::infinity()}},Json{{"fontSize",std::numeric_limits<std::uint64_t>::max()}}};
 for(const auto* key:{"bold","italic","underline"})invalid.push_back(Json{{key,"true"}});
 for(const auto* key:{"fontFamily","fill","color","alignment","numberFormat","note"})invalid.push_back(Json{{key,17}});
 for(const auto& style:invalid){
  WorkbookSession session;const auto before=session.snapshot();std::string error;
  const Json operations={{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","must roll back"}},{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",style}}};
  require(!session.apply(0,operations,&error),"malformed cell style must be rejected before commit");
  require(session.snapshot()==before&&!session.canUndo()&&!error.empty(),"rejected style batch must preserve cells revision and undo");
  require(session.preview(0,operations,&error).is_null()&&!session.hasPreview(),"malformed style preview must not become acceptable");
  auto imported=before;imported["sheets"][0]["cells"]={{{"row",1},{"col",1},{"input","1"},{"style",style}}};bool rejected=false;try{WorkbookSession loaded(imported);}catch(const std::exception&){rejected=true;}require(rejected,"imported cell styles must be validated before rendering");
 }
 WorkbookSession valid;const Json style={{"fontSize",11.5},{"bold",true},{"fontFamily","Calibri"},{"numberFormat","0.00"},{"packageExtension",{{"opaque",17}}}};
 require(valid.apply(0,Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",style}}})),"valid numeric imported font sizes must remain supported");
 require(valid.snapshot()["sheets"][0]["cells"][0]["style"]==style,"unknown package style fields must remain preserved");
}
template<class Session> Json takeRecovery(Session& session){if constexpr(requires { session.takeRecoveryDelta(); })return session.takeRecoveryDelta();else throw std::runtime_error("Session must expose ordered transaction recovery deltas");}
int main(){try{
 chartPixelOffsetsAreValidatedAtomically();
 malformedChartMetadataIsRejectedAtomically();
 chartMetadataBoundariesAreValidated();
 malformedVbaMetadataIsRejectedAtomically();
 malformedCellStylesAreRejectedAtomically();
 WorkbookSession session;
 const auto initial=session.snapshot();
 require(initial.at("revision")==0,"new workbook revision");
 require(initial.at("sheets").size()==1,"new workbook sheet");
 const auto id=initial.at("sheets")[0].at("id").get<std::string>();
 std::string error;
 require(session.apply(0,Json::array({{{"type","set_cell"},{"sheet",id},{"row",1},{"col",1},{"input","42"}}}),&error),"cell transaction");
 require(session.snapshot().at("sheets")[0].at("cells")[0].at("input")=="42","cell input stored");
 require(session.snapshot().at("revision")==1,"revision increments");
 require(!session.apply(0,Json::array(),&error),"stale revision must fail");
 const auto before=session.snapshot();
 require(!session.apply(1,Json::array({{{"type","set_cell"},{"sheet",id},{"row",2},{"col",1},{"input","first"}},{{"type","set_cell"},{"sheet",id},{"row",0},{"col",1},{"input","invalid"}}}),&error),"invalid row must fail");
 require(session.snapshot()==before,"failed batch must be atomic");
 require(!session.apply(1,Json::object(),&error),"operations must be array");
 require(session.canUndo(),"transaction has undo");
 require(session.undo(&error),"undo succeeds");
 require(session.snapshot().at("sheets")[0].at("cells").empty(),"undo restores cells");
 require(session.snapshot().at("revision")==2,"undo revision increases");
 require(session.redo(&error),"redo succeeds");
 require(session.snapshot().at("revision")==3,"redo revision increases");
 const auto proposal=session.preview(3,Json::array({{{"type","set_cell"},{"sheet",id},{"row",1},{"col",1},{"input","99"}}}),&error);
 require(!proposal.is_null()&&proposal.at("sheets")[0].at("cells")[0].at("input")=="99","preview has candidate cells");
 require(session.snapshot().at("sheets")[0].at("cells")[0].at("input")=="42","preview is isolated");
 require(session.hasPreview(),"preview exists");
 require(session.accept(&error),"preview acceptance succeeds");
 require(session.snapshot().at("sheets")[0].at("cells")[0].at("input")=="99","accepted preview is live");
 require(session.undo(&error),"accepted preview is one undo");
 require(session.snapshot().at("sheets")[0].at("cells")[0].at("input")=="42","preview undo restores live cells");
 session.preview(5,Json::array({{{"type","set_cell"},{"sheet",id},{"row",1},{"col",1},{"input","100"}}}),&error);
 session.discard();require(!session.hasPreview(),"discard removes preview");
 const auto rev=session.snapshot().at("revision").get<std::uint64_t>();
 require(session.apply(rev,Json::array({{{"type","add_sheet"},{"id","forecast"},{"name","Forecast"}},{{"type","rename_sheet"},{"sheet","forecast"},{"name","Budget"}},{{"type","set_cell"},{"sheet","forecast"},{"row",7},{"col",3},{"input","=42+1"}},{{"type","set_style"},{"sheet","forecast"},{"row",7},{"col",3},{"style",{{"bold",true},{"numberFormat","0.00"}}}}}),&error),"sheet and style operations");
 require(session.snapshot().at("sheets")[1].at("name")=="Budget","sheet renamed");
 require(session.snapshot().at("sheets")[1].at("cells")[0].at("style").at("bold")==true,"style stored");
 require(!session.apply(rev+1,Json::array({{{"type","add_sheet"},{"id","other"},{"name","budget"}}}),&error),"duplicate sheet names rejected case insensitively");
 require(session.apply(rev+1,Json::array({{{"type","clear_range"},{"sheet","forecast"},{"row1",1},{"col1",1},{"row2",10},{"col2",10}}}),&error),"clear range");
 require(session.snapshot().at("sheets")[1].at("cells")[0].at("input")=="","clear contents retains formatting");
 const auto metaRev=session.snapshot().at("revision").get<std::uint64_t>();
 require(session.apply(metaRev,Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({{{"id","chart-1"},{"type","line"}}})}}}),&error),"chart metadata transaction");
 require(session.snapshot().at("charts").size()==1,"chart metadata retained");
 require(!session.apply(metaRev+1,Json::array({{{"type","set_metadata"},{"key","revision"},{"value",999}}}),&error),"metadata cannot forge revisions");
 const auto pasteRev=session.snapshot()["revision"].get<std::uint64_t>();
 require(session.apply(pasteRev,Json::array({{{"type","set_style"},{"sheet",id},{"row",1},{"col",1},{"style",{{"bold",true},{"fill","#FFFF00"}}}},{{"type","set_style"},{"sheet",id},{"row",1},{"col",1},{"style",{{"italic",true}}},{"replace",true}}})),"replace destination formatting");
 require(!session.snapshot()["sheets"][0]["cells"][0]["style"].contains("bold"),"format paste replaces old style");
 require(session.apply(pasteRev+1,Json::array({{{"type","replace_sheet_cells"},{"sheet",id},{"cells",Json::array({{{"row",3},{"col",2},{"input","moved"}}})}}})),"structural cells replacement");
 require(session.snapshot()["sheets"][0]["cells"].size()==1&&session.snapshot()["sheets"][0]["cells"][0]["row"]==3,"replacement is complete");
 require(session.apply(pasteRev+2,Json::array({{{"type","delete_sheet"},{"sheet","forecast"}}})),"delete second sheet");
 require(!session.apply(pasteRev+3,Json::array({{{"type","delete_sheet"},{"sheet",id}}})),"cannot delete last worksheet");
 WorkbookSession renamed;require(renamed.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","=Sheet1!A1+'Sheet1'!$B$2+\"Sheet1!A1\"+'[Other.xlsx]Sheet1'!A1"}},{{"type","set_metadata"},{"key","names"},{"value",{{"Total","=SUM(Sheet1!A1:A5)"}}}}})),"rename fixture");
 require(renamed.apply(1,Json::array({{{"type","rename_sheet"},{"sheet","sheet-1"},{"name","Forecast FY'26"}}})),"rename sheet");
 require(renamed.snapshot()["sheets"][0]["cells"][0]["input"]=="='Forecast FY''26'!A1+'Forecast FY''26'!$B$2+\"Sheet1!A1\"+'[Other.xlsx]Sheet1'!A1","sheet rename updates local references, escaping names and preserving strings/external links");
 require(renamed.snapshot()["names"]["Total"]=="=SUM('Forecast FY''26'!A1:A5)","sheet rename updates workbook named references");
 require(!renamed.apply(2,Json::array({{{"type","rename_sheet"},{"sheet","sheet-1"},{"name","Bad\nSheet"}}})),"worksheet names reject control characters");
 WorkbookSession journal;require(takeRecovery(journal).empty(),"new session has no recovery deltas");require(journal.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","one"}}})),"journal first edit");require(journal.apply(1,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","two"}}})),"journal second edit");auto forward=takeRecovery(journal);require(forward.size()==2&&forward[0]["baseRevision"]==0&&forward[0]["revision"]==1&&forward[1]["baseRevision"]==1&&forward[1]["revision"]==2&&forward[1]["changes"]["cells"][0]["cell"]["input"]=="two","undrained recovery deltas retain every ordered committed edit");require(takeRecovery(journal).empty(),"taking deltas consumes the queue");require(journal.undo(),"journal undo");auto backwards=takeRecovery(journal);require(backwards.size()==1&&backwards[0]["baseRevision"]==2&&backwards[0]["revision"]==3&&backwards[0]["changes"]["cells"][0]["cell"]["input"]=="one","undo recovery delta stores actual inverse changes with monotonic revision");require(journal.redo(),"journal redo");require(takeRecovery(journal)[0]["changes"]["cells"][0]["cell"]["input"]=="two","redo recovery delta stores actual forward changes");journal.preview(4,Json::array({{{"type","add_sheet"},{"id","extra"},{"name","Extra"}}}));require(takeRecovery(journal).empty(),"preview produces no durable recovery delta");require(journal.accept(),"journal accept");auto accepted=takeRecovery(journal);require(accepted.size()==1&&accepted[0]["patch"]==true&&accepted[0]["baseRevision"]==4&&accepted[0]["revision"]==5&&accepted[0]["changes"].is_array(),"accepted structural preview emits its RFC6902 recovery changes");
 std::cout<<"session tests passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
