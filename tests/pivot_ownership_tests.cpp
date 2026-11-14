#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
Json snapshot(){return {{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Source"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Region"}},{{"row",1},{"col",2},{"input","Amount"}},{{"row",2},{"col",1},{"input","East"}},{{"row",2},{"col",2},{"input","10"}}})}},{{"id","r"},{"name","Report"},{"cells",Json::array()}}})}};}
Json definition(){return {{"id","p1"},{"name","Report1"},{"source",{{"sheet","s"},{"row1",1},{"col1",1},{"row2",2},{"col2",2}}},{"rows",Json::array({{{"field","Region"}}})},{"values",Json::array({{{"field","Amount"},{"aggregate","sum"}}})},{"output",{{"sheet","r"},{"row1",1},{"col1",1}}}};}
PivotGeneration generation(const Json& book,const Json& spec,std::uint64_t context=1){return generatePivot(book,{{"revision",book.at("revision")},{"status","ok"},{"values",Json::array()}},spec,context);}
void unchangedRefresh(){auto book=snapshot();auto generated=generation(book,definition());applyPivotTransaction(book,pivotOperations(book,generated)[0]);book["revision"]=1;generated=generation(book,definition(),2);
 require(pivotOperations(book,generated).empty(),"unchanged refresh ignores revision/context bookkeeping and produces no transaction");
 book["pivots"][0]["stale"]=true;require(!pivotOperations(book,generated).empty(),"clearing stale state remains a meaningful refresh");book["pivots"][0]["stale"]=false;
 generated.definition["provenance"]={{"allowCachedLinks",true}};require(!pivotOperations(book,generated).empty(),"changed link provenance remains a meaningful refresh");
}
void reportSourceIsolation(){for(bool sourceInOutput:{true,false}){auto book=snapshot();auto first=generation(book,definition());applyPivotTransaction(book,pivotOperations(book,first)[0]);auto next=definition();next["id"]="p2";next["name"]="Report2";
 if(sourceInOutput){next["source"]["sheet"]="r";next["output"]={{"sheet","s"},{"row1",10},{"col1",1}};}
 else{book["sheets"].push_back({{"id","d"},{"name","OtherSource"},{"cells",book["sheets"][0]["cells"]}});book["sheets"][0]["cells"]=Json::array();next["source"]["sheet"]="d";next["output"]={{"sheet","s"},{"row1",1},{"col1",1}};}
 const auto generated=generation(book,next);bool rejected=false;try{pivotOperations(book,generated);}catch(const std::exception& e){rejected=std::string(e.what()).find("source")!=std::string::npos;}require(rejected,"one pivot's generated output cannot overlap another pivot's source in either direction");
}}
void sourceFormatInvalidation(){auto book=snapshot();auto generated=generation(book,definition());applyPivotTransaction(book,pivotOperations(book,generated)[0]);
 invalidatePivots(book,{{"type","set_style"},{"sheet","s"},{"row",2},{"col",2},{"style",{{"numberFormat","0.00"}}}});require(book["pivots"][0]["stale"]==true,"source number-format changes mark the report cache stale");book["pivots"][0]["stale"]=false;
 invalidatePivots(book,{{"type","set_style"},{"sheet","r"},{"row",2},{"col",2},{"style",{{"numberFormat","0.00"}}}});require(book["pivots"][0]["stale"]==false,"formatting generated output does not stale its source cache");
 invalidatePivots(book,{{"type","set_style"},{"sheet","s"},{"row",2},{"col",2},{"style",{{"bold",true}}}});require(book["pivots"][0]["stale"]==false,"unrelated source decoration does not stale a report");
}
int main(){try{unchangedRefresh();reportSourceIsolation();sourceFormatInvalidation();std::cout<<"pivot ownership tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
