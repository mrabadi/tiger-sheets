#include "tigersheets/worksheet_layout.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 Json book={{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Sheet1"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Revenue"}},{{"row",1},{"col",2},{"input","Keep me"}}})}}})}};
 const Json merges=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",3}}});
 applyWorksheetLayout(book,{{"type","set_sheet_layout"},{"sheet","s"},{"mergedRanges",merges},{"rows",Json::array({{{"index",2},{"size",30.0},{"hidden",true}}})},{"columns",Json::array({{{"index",3},{"size",18.5}}})}});
 const auto& sheet=book.at("sheets")[0];require(sheet.at("mergedRanges")==merges,"merge rectangles are retained");require(sheet.at("rowLayout")[0].at("size")==30.0&&sheet.at("rowLayout")[0].at("hidden")==true,"row size and hidden state retained");require(sheet.at("columnLayout")[0].at("size")==18.5,"column width retained");require(sheet.contains("hiddenRows")&&sheet.at("hiddenRows")==Json::array({2}),"hidden rows are passed through to SUBTOTAL calculation");require(sheet.at("cells")[1].at("input")=="Keep me","merge layout alone must not silently clear cell values");
 const auto before=book;bool rejected=false;try{applyWorksheetLayout(book,{{"type","set_sheet_layout"},{"sheet","s"},{"mergedRanges",Json::array({merges[0],{{"row1",2},{"col1",3},{"row2",4},{"col2",4}}})}});}catch(const std::exception&){rejected=true;}require(rejected&&book==before,"overlapping merges reject atomically");
 for(const auto& patch:std::vector<Json>{
  {{"mergedRanges",Json::array({{{"row1",0},{"col1",1},{"row2",2},{"col2",3}}})}},
  {{"mergedRanges",Json::array({{{"row1",1},{"col1",1},{"row2",1},{"col2",1}}})}},
  {{"mergedRanges",Json::array({{{"row1",1},{"col1",1},{"row2",1048577},{"col2",3}}})}},
  {{"rows",Json::array({{{"index",1},{"size",500.0}}})}},
  {{"columns",Json::array({{{"index",16385},{"hidden",true}}})}},
  {{"columns",Json::array({{{"index",3},{"hidden",true}},{{"index",3},{"size",12.0}}})}},
  {{"rows",Json::array({{{"index",1},{"hidden","yes"}}})}},
  {{"rows",Json::array({{{"index",1},{"unrelated",99}}})}},
  {{"defaultColumnWidth",0.0}},
  {{"defaultColumnWidth",256.0}},
  {{"defaultColumnWidth","wide"}},
  {{"unrelated",42}}
 }){auto operation=patch;operation["type"]="set_sheet_layout";operation["sheet"]="s";bool invalid=false;try{applyWorksheetLayout(book,operation);}catch(const std::exception&){invalid=true;}require(invalid&&book==before,"invalid or unknown layout properties reject atomically");}
 for(const auto* collection:{"pivots","financialModels","tables"}){auto owned=before;owned[collection]=Json::array({{{"id","owned"},{"source",{{"sheet","s"},{"row1",5},{"col1",1},{"row2",7},{"col2",4}}},{"output",{{"sheet","s"},{"row1",10},{"col1",1},{"row2",12},{"col2",4}}}}});const auto original=owned;bool protectedRange=false;try{applyWorksheetLayout(owned,{{"type","set_sheet_layout"},{"sheet","s"},{"mergedRanges",Json::array({{{"row1",5},{"col1",1},{"row2",5},{"col2",2}}})}});}catch(const std::exception&){protectedRange=true;}require(protectedRange&&owned==original,"merge cannot destroy report/table source semantics");}
 auto nativeTable=before;nativeTable["tables"]=Json::array({{{"id","table"},{"sheet","s"},{"range",{{"row1",5},{"col1",1},{"row2",8},{"col2",3}}}}});const auto nativeTableBefore=nativeTable;bool tableRange=false;try{applyWorksheetLayout(nativeTable,{{"type","set_sheet_layout"},{"sheet","s"},{"mergedRanges",Json::array({{{"row1",5},{"col1",1},{"row2",5},{"col2",2}}})}});}catch(const std::exception&){tableRange=true;}require(tableRange&&nativeTable==nativeTableBefore,"table-owned range inherits its worksheet identity");
 auto shifted=before.at("sheets")[0];rebaseWorksheetLayout(shifted,true,2,2,true);require(shifted.at("mergedRanges")[0].at("row1")==1&&shifted.at("mergedRanges")[0].at("row2")==4,"inserting inside merged range expands it");require(shifted.at("rowLayout")[0].at("index")==4&&shifted.at("hiddenRows")==Json::array({4}),"row dimensions and hidden calculation state move with insertion");rebaseWorksheetLayout(shifted,true,1,3,false);require(shifted.at("mergedRanges")[0].at("row1")==1&&shifted.at("mergedRanges")[0].at("row2")==1,"partial deletion shrinks merge");require(shifted.at("rowLayout")[0].at("index")==1,"remaining row size follows deletion");rebaseWorksheetLayout(shifted,false,2,1,false);require(shifted.at("mergedRanges")[0].at("col2")==2&&shifted.at("columnLayout")[0].at("index")==2,"column deletion shrinks merge and moves dimension");rebaseWorksheetLayout(shifted,false,1,2,false);require(shifted.at("mergedRanges").empty()&&shifted.at("columnLayout").empty(),"deleting complete merge and dimensions removes definitions");
 auto boundary=before.at("sheets")[0];boundary["columnLayout"]=Json::array({{{"index",16384},{"size",12.0}}});const auto bounded=boundary;bool overflow=false;try{rebaseWorksheetLayout(boundary,false,1,1,true);}catch(const std::exception&){overflow=true;}require(overflow&&boundary==bounded,"dimension insertion overflow rejects atomically");
 std::cout<<"Worksheet layout tests passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
