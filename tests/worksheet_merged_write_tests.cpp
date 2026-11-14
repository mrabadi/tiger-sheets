#include "tigersheets/session.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json mergedFixture(){return {{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Sheet1"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Revenue"}}})},{"mergedRanges",Json::array({{{"row1",1},{"col1",1},{"row2",1},{"col2",2}}})}}})}};}
int main(){try{
 for(const bool mixed:{false,true}){
  WorkbookSession session(mergedFixture());const auto before=session.snapshot();const auto change=session.lastChange();const auto bytes=session.historyBytes();Json operations=Json::array();if(mixed)operations.push_back({{"type","set_metadata"},{"key","settings"},{"value",{{"calcMode","manual"}}}});
  operations.push_back({{"type","set_cell"},{"sheet","s"},{"row",1},{"col",1},{"input","Changed anchor"}});operations.push_back({{"type","set_cell"},{"sheet","s"},{"row",1},{"col",2},{"input","Hidden value"}});std::string error;
  require(!session.apply(0,operations,&error)&&error.find("merged")!=std::string::npos,"writing a non-anchor merged cell must reject with an actionable error");require(session.snapshot()==before&&session.lastChange()==change&&!session.canUndo()&&!session.canRedo()&&session.historyBytes()==bytes,"forbidden interior write cannot commit an earlier anchor edit or history entry");require(session.takeRecoveryDelta().empty(),"rejected merged write cannot publish recovery deltas");
 }
 for(const bool mixed:{false,true}){
  auto imported=mergedFixture();imported["sheets"][0]["cells"].push_back({{"row",1},{"col",2},{"input","Imported hidden value"}});WorkbookSession session(imported);require(session.snapshot()==imported,"imported non-anchor values remain readable and preserved before any edit");Json operations=Json::array();if(mixed)operations.push_back({{"type","set_metadata"},{"key","settings"},{"value",{{"calcMode","manual"}}}});
  operations.push_back({{"type","set_cell"},{"sheet","s"},{"row",1},{"col",1},{"input","Updated anchor"}});operations.push_back({{"type","set_style"},{"sheet","s"},{"row",1},{"col",2},{"style",{{"bold",true}}}});operations.push_back({{"type","set_cell"},{"sheet","s"},{"row",1},{"col",2},{"input",""}});std::string error;require(session.apply(0,operations,&error),error.c_str());const auto accepted=session.snapshot();require(session.cellAt("s",1,1)->at("input")=="Updated anchor"&&session.cellAt("s",1,2)->at("input")==""&&session.cellAt("s",1,2)->at("style").at("bold")==true,"anchor edits, explicit interior clears and interior formatting remain supported");require(session.undo(&error),error.c_str());require(session.snapshot().at("sheets")==imported.at("sheets"),"undo can restore original imported hidden values without invoking write restrictions");require(session.redo(&error),error.c_str());require(session.snapshot().at("sheets")==accepted.at("sheets"),"redo restores the accepted clear and formatting");
 }
 std::cout<<"Worksheet merged write tests passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
