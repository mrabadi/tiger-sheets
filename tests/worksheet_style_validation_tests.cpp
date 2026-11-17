#include "tigersheets/session.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace tigersheets;
namespace {
void require(bool okay,const std::string& message){if(!okay)throw std::runtime_error(message);}
Json fixture(){auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","7"},{"style",{{"bold",true}}}}});return book;}
void rejects(const Json& styles){for(const auto& style:styles){
 WorkbookSession cells(fixture());const auto before=cells.snapshot();const auto id=before.at("sheets")[0].at("id");std::string error;const Json change={{"type","set_cell"},{"sheet",id},{"row",1},{"col",1},{"input","8"}};
 require(!cells.apply(0,Json::array({change,{{"type","set_style"},{"sheet",id},{"row",1},{"col",1},{"style",style}}}),&error)&&cells.snapshot()==before&&!cells.canUndo(),"Malformed cell style must reject atomically before reaching rendering: "+style.dump());
 WorkbookSession defaults(fixture());require(!defaults.apply(0,Json::array({change,{{"type","set_sheet_layout"},{"sheet",id},{"defaultStyle",style}}}),&error)&&defaults.snapshot()==before&&!defaults.canUndo(),"Malformed default style must reject atomically: "+style.dump());
 auto imported=fixture();imported["sheets"][0]["cells"][0]["style"].update(style);bool rejected=false;try{WorkbookSession invalid(imported);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed imported cell style must reject: "+style.dump());
 imported=fixture();imported["sheets"][0]["defaultStyle"]=style;rejected=false;try{WorkbookSession invalid(imported);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed imported default style must reject: "+style.dump());
}}
void indent(){rejects(Json::array({Json{{"indent","bad"}},Json{{"indent",-1}},Json{{"indent",251}},Json{{"indent",1.5}},Json{{"indent",true}},Json{{"indent",nullptr}}}));}
void border(){const Json edge={{"style","thin"},{"color","#000000"}};rejects(Json::array({Json{{"border",true}},Json{{"border",Json::array()}},Json{{"border",{{"diagonal",edge}}}},Json{{"border",{{"top",nullptr}}}},Json{{"border",{{"top",true}}}},Json{{"border",{{"top",{{"style",true},{"color","#000000"}}}}}},Json{{"border",{{"top",{{"style","invented"},{"color","#000000"}}}}}},Json{{"border",{{"top",{{"style","thin"},{"color",17}}}}}},Json{{"border",{{"top",{{"style","thin"},{"color","not-a-color"}}}}}},Json{{"border",{{"top",{{"style","thin"},{"color","#GG0000"}}}}}},Json{{"border",{{"top",{{"style","thin"}}}}}},Json{{"border",{{"top",{{"style","thin"},{"color","#000000"},{"width",2}}}}}}}));}
void vertical(){rejects(Json::array({Json{{"verticalAlignment",true}},Json{{"verticalAlignment","middle"}},Json{{"verticalAlignment",""}},Json{{"verticalAlignment",nullptr}}}));}
void wrap(){rejects(Json::array({Json{{"wrap","true"}},Json{{"wrap",1}},Json{{"wrap",nullptr}}}));}
void valid(){for(const int indentValue:{0,250})for(const auto* alignment:{"top","center","bottom","justify","distributed"})for(const auto* line:{"thin","medium","dashed","dotted","thick","double","hair","mediumDashed","dashDot","mediumDashDot","dashDotDot","mediumDashDotDot","slantDashDot","none"}){
 Json style={{"indent",indentValue},{"verticalAlignment",alignment},{"wrap",false},{"border",{{"top",{{"style",line},{"color","#A0b1C2"}}}}}};WorkbookSession session(fixture());const auto id=session.snapshotView().at("sheets")[0].at("id");std::string error;require(session.apply(0,Json::array({{{"type","set_style"},{"sheet",id},{"row",1},{"col",1},{"style",style}}}),&error),"Valid imported line styles and boundary alignment fields remain accepted: "+error);require(session.apply(1,Json::array({{{"type","set_sheet_layout"},{"sheet",id},{"defaultStyle",style}}}),&error),"Valid default style remains accepted: "+error);
 }WorkbookSession empty(fixture());const auto id=empty.snapshotView().at("sheets")[0].at("id");require(empty.apply(0,Json::array({{{"type","set_style"},{"sheet",id},{"row",1},{"col",1},{"style",{{"border",Json::object()},{"wrap",true}}}}})),"Empty borders and enabled wrap remain valid");}
}
int main(int argc,char** argv){try{const std::string test=argc>1?argv[1]:"all";if(test=="indent")indent();else if(test=="border")border();else if(test=="vertical")vertical();else if(test=="wrap")wrap();else{indent();border();vertical();wrap();valid();}std::cout<<"Core worksheet style validation "<<test<<" passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
