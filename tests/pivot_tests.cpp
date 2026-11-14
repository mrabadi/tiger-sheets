#include <limits>
#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include "tigersheets/calculation.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
Json source(){return {{"revision",7},{"sheets",Json::array({{{"id","s"},{"name","Sales"},{"cells",Json::array({
 {{"row",1},{"col",1},{"input","Region"}},{{"row",1},{"col",2},{"input","Revenue"}},
 {{"row",2},{"col",1},{"input","East"}},{{"row",2},{"col",2},{"input","10"}},
 {{"row",3},{"col",1},{"input","West"}},{{"row",3},{"col",2},{"input","20"}},
 {{"row",4},{"col",1},{"input","East"}},{{"row",4},{"col",2},{"input","30"}}
 })}},{{"id","r"},{"name","Report"},{"cells",Json::array()}}})}};}
Json calculated(){return {{"revision",7},{"status","ok"},{"converged",true},{"values",Json::array({
 {{"sheet","s"},{"row",2},{"col",2},{"type","number"},{"value",10}},
 {{"sheet","s"},{"row",3},{"col",2},{"type","number"},{"value",20}},
 {{"sheet","s"},{"row",4},{"col",2},{"type","number"},{"value",30}}
 })}};}
Json spec(){return {{"id","p1"},{"name","RevenueReport"},{"source",{{"sheet","s"},{"row1",1},{"col1",1},{"row2",4},{"col2",2}}},
 {"rows",Json::array({{{"field","Region"}}})},{"columns",Json::array()},{"filters",Json::array()},
 {"values",Json::array({{{"field","Revenue"},{"aggregate","sum"},{"name","Revenue"},{"numberFormat","#,##0.00"}}})},
 {"output",{{"sheet","r"},{"row1",1},{"col1",1}}}};}
void sumReport(){const auto generated=generatePivot(source(),calculated(),spec(),12);
 require(generated.definition.at("version")==1,"native pivot schema version");
 require(generated.definition.at("cache").at("records").size()==3,"cache retains every source record");
 require(generated.definition.at("cache").at("contextGeneration")==12,"cache identifies frozen link context");
 require(generated.definition.at("layout").at("data")[0][0].at("value")==40,"East sum is 40");
 require(generated.definition.at("layout").at("data")[1][0].at("value")==20,"West sum is 20");
 require(generated.definition.at("output").at("row2")==4,"header, two members, grand total");
 require(generated.cells.back().at("input")=="60.0"||generated.cells.back().at("input")=="60","grand total is 60");
}
void columnReport(){auto book=source();auto& cells=book["sheets"][0]["cells"];for(int row=1;row<=4;++row)cells.push_back({{"row",row},{"col",3},{"input",row==1?"Period":row==3?"2026-02":"2026-01"}});
 auto definition=spec();definition["source"]["col2"]=3;definition["columns"]=Json::array({{{"field","Period"}}});definition["values"]=Json::array();
 for(const auto* aggregate:{"average","min","max","count"})definition["values"].push_back({{"field","Revenue"},{"aggregate",aggregate},{"name",aggregate},{"numberFormat","0.00"}});
 auto generated=generatePivot(book,calculated(),definition);const auto& layout=generated.definition.at("layout");
 require(layout.at("columnKeys").size()==2,"column fields produce distinct periods");
 require(layout.at("data")[0][0].at("value")==20,"average uses underlying numeric records");
 require(layout.at("data")[0][1].at("value")==10,"minimum uses underlying records");
 require(layout.at("data")[0][2].at("value")==30,"maximum uses underlying records");
 require(layout.at("data")[0][3].at("value")==2,"count includes both records");
 require(layout.at("grandTotals")[0].at("value")==20,"grand average is 60/3");
 require(generated.definition.at("output").at("col2")==13,"four measures across periods plus row totals");
}
void typedFilters(){auto book=source();auto& cells=book["sheets"][0]["cells"];cells[2]["input"]="'00123";cells[4]["input"]="123";cells[6]["input"]="";
 auto definition=spec();definition["filters"]=Json::array({{{"field","Region"},{"all",false},{"items",Json::array({{{"type","string"},{"value","00123"}},{{"type","blank"},{"value",nullptr}},{{"type","string"},{"value","Gone"}}})}}});
 auto generated=generatePivot(book,calculated(),definition);
 require(generated.definition["layout"]["rowKeys"].size()==2,"explicit typed filter includes text and blank but excludes numeric member");
 require(generated.definition["layout"]["grandTotals"][0]["value"]==40,"filtered total uses selected source records");
 require(generated.definition["cache"]["records"].size()==3,"filters retain full cache for future changes");
 require(generated.definition["filters"][0]["missingItems"].size()==1,"disappeared selected member remains visible as missing");
 definition["filters"][0]["all"]=true;require(generatePivot(book,calculated(),definition).definition["layout"]["grandTotals"][0]["value"]==60,"All admits numeric and new members");
}
void rejectsUnqualifiedSources(){for(const auto* mode:{"stale","nonconverged","error"}){auto result=calculated();if(std::string(mode)=="stale")result["revision"]=6;if(std::string(mode)=="nonconverged")result["iteration"]={{"converged",false}};if(std::string(mode)=="error")result["values"][0]={{"sheet","s"},{"row",2},{"col",2},{"type","error"},{"value","#DIV/0!"}};bool rejected=false;try{generatePivot(source(),result,spec());}catch(const std::exception&){rejected=true;}require(rejected,"stale, nonconverged or error sources cannot publish a financial report");}}
void dateGroups(){auto book=source();auto& cells=book["sheets"][0]["cells"];cells[0]["input"]="Date";cells[2]["input"]="45292";cells[4]["input"]="45383";cells[6]["input"]="45658";
 auto definition=spec();definition["rows"]=Json::array({{{"field","Date"},{"group","year"}},{{"field","Date"},{"group","quarter"}},{{"field","Date"},{"group","month"}}});
 auto generated=generatePivot(book,calculated(),definition);const auto& keys=generated.definition["layout"]["rowKeys"];
 require(keys[0][0]["value"]==2024&&keys[0][1]["value"]==1&&keys[0][2]["value"]==1,"January date decomposes into chronological numeric components");
 require(keys[1][0]["value"]==2024&&keys[1][1]["value"]==2&&keys[1][2]["value"]==4,"April is quarter two");
 require(keys[2][0]["value"]==2025,"following year sorts after previous year");
 cells[2]["input"]="not a date";bool rejected=false;try{generatePivot(book,calculated(),definition);}catch(const std::exception&){rejected=true;}require(rejected,"explicit grouping rejects ambiguous date text");
}
void numericDateOrder(){auto book=source();auto& cells=book["sheets"][0]["cells"];cells[0]["input"]="Date";cells[2]["input"]="300";cells[4]["input"]="61";cells[6]["input"]="100";auto definition=spec();definition["rows"]=Json::array({{{"field","Date"},{"group","month"}}});auto generated=generatePivot(book,calculated(),definition);const auto& keys=generated.definition["layout"]["rowKeys"];require(keys[0][0]["value"]==3&&keys[1][0]["value"]==4&&keys[2][0]["value"]==10,"date member sorting is numeric, not lexical");}
void sourceLimits(){for(const auto* problem:{"records","measures","headers"}){auto book=source();auto definition=spec();if(std::string(problem)=="records")definition["source"]["row2"]=100002;if(std::string(problem)=="measures")for(int i=0;i<4;++i)definition["values"].push_back(definition["values"][0]);if(std::string(problem)=="headers"){book["sheets"][0]["cells"][1]["input"]="Region";definition["values"][0]["field"]="Region";}bool rejected=false;try{generatePivot(book,calculated(),definition);}catch(const std::exception&){rejected=true;}require(rejected,"record/measure limits and unique headers are enforced before report generation");}}
void ownedRefresh(){auto book=source();auto generated=generatePivot(book,calculated(),spec());const auto operations=pivotOperations(book,generated);require(operations[0]["type"]=="replace_pivot","report uses one owned bulk transaction");applyPivotTransaction(book,operations[0]);
 require(book["pivots"].size()==1&&book["sheets"][1]["cells"].size()==8,"cache definition and output commit together");
 bool rejected=false;try{validatePivotEdit(book,{{"type","set_cell"},{"sheet","r"},{"row",2},{"col",2},{"input","999"}});}catch(const std::exception&){rejected=true;}require(rejected,"generated results cannot be overwritten as ordinary values");
 book["sheets"][0]["cells"][4]["input"]="East";generated=generatePivot(book,calculated(),spec());applyPivotTransaction(book,pivotOperations(book,generated)[0]);
 require(book["pivots"].size()==1&&book["sheets"][1]["cells"].size()==6,"shrinking refresh clears obsolete owned cells without duplicate definitions");
 auto occupied=source();occupied["sheets"][1]["cells"].push_back({{"row",2},{"col",2},{"input","protected"}});rejected=false;try{pivotOperations(occupied,generatePivot(occupied,calculated(),spec()));}catch(const std::exception&){rejected=true;}require(rejected,"report creation refuses occupied unowned output");
}
void reportLookup(){auto book=source();auto generated=generatePivot(book,calculated(),spec());applyPivotTransaction(book,pivotOperations(book,generated)[0]);
 auto value=getPivotData(book,"r",2,2,"Revenue",Json::array({{{"field","Region"},{"item",{{"type","string"},{"value","East"}}}}}));require(value["type"]=="number"&&value["value"]==40,"GETPIVOTDATA retrieves a visible member by pivot identity");
 require(getPivotData(book,"r",1,1,"Revenue")["value"]==60,"GETPIVOTDATA returns the grand total");
 require(getPivotData(book,"r",20,1,"Revenue")["value"]=="#REF!","lookup outside a pivot returns REF");
 auto filtered=spec();filtered["filters"]=Json::array({{{"field","Region"},{"all",false},{"items",Json::array({{{"type","string"},{"value","East"}}})}}});generated=generatePivot(book,calculated(),filtered);applyPivotTransaction(book,pivotOperations(book,generated)[0]);
 require(getPivotData(book,"r",1,1,"Revenue",Json::array({{{"field","Region"},{"item",{{"type","string"},{"value","West"}}}}}))["value"]=="#REF!","filtered-out members cannot be retrieved");
}
void linkedPivotChart(){auto book=source();auto generated=generatePivot(book,calculated(),spec());applyPivotTransaction(book,pivotOperations(book,generated)[0]);auto chart=pivotChart(book,"p1","pc1","line");require(chart["pivotId"]=="p1","chart retains owning pivot identity");auto data=pivotChartData(book,chart);require(data["categories"]==Json::array({"East","West"}),"chart includes members, excludes Grand Total");require(data["series"][0]["values"]==Json::array({40,20}),"chart values derive from the committed pivot generation");bool rejected=false;try{pivotChart(book,"p1","pc2","scatter");}catch(const std::exception&){rejected=true;}require(rejected,"unsupported pivot chart families are refused");}
void nativeEngineTextTypes(){auto result=calculated();result["values"].push_back({{"sheet","s"},{"row",1},{"col",1},{"type","text"},{"value","Region"}});result["values"].push_back({{"sheet","s"},{"row",1},{"col",2},{"type","text"},{"value","Revenue"}});result["values"].push_back({{"sheet","s"},{"row",2},{"col",1},{"type","text"},{"value","00123"}});result["values"].push_back({{"sheet","s"},{"row",3},{"col",1},{"type","empty"},{"value",nullptr}});auto report=generatePivot(source(),result,spec());require(report.definition["cache"]["fields"][0]["items"][0]["type"]=="string","native text results retain literal string type in pivot caches");require(report.definition["cache"]["fields"][0]["items"][1]["type"]=="blank","native empty results remain distinct blanks");}
void cacheRendering(){auto generated=generatePivot(source(),calculated(),spec(),17);auto cached=generatePivotFromCache(source(),spec(),generated.definition["cache"]["fields"],generated.definition["cache"]["records"],17);require(cached.cells==generated.cells&&cached.definition==generated.definition,"native cached records render identically without reconstructing worksheet cells");}
void sourceCachePreparation(){
 auto book=source();auto result=calculated();book["sheets"][0]["cells"][3]["input"]="=100+5";result["values"][0]["value"]=105;
 const auto expected=generatePivot(book,result,spec(),17);auto cache=buildPivotSourceCache(book,result,spec());
 require(cache.fields==expected.definition["cache"]["fields"]&&cache.records==expected.definition["cache"]["records"],"source cache preparation preserves current formula values and typed field dictionaries");
 auto minimal=book;for(auto& sheet:minimal["sheets"])sheet["cells"]=Json::array();const auto compact=generatePivotFromCache(minimal,spec(),std::move(cache.fields),std::move(cache.records),17);
 require(compact.definition==expected.definition&&compact.cells==expected.cells,"a compact source cache renders independently of annotated source cells");
 for(const auto* mode:{"stale","nonconverged","formula_error","cancelled"}){auto invalid=result;if(std::string(mode)=="stale")invalid["revision"]=6;if(std::string(mode)=="nonconverged")invalid["iteration"]={{"converged",false}};if(std::string(mode)=="formula_error")invalid["values"][0]["type"]="error";bool refused=false;try{buildPivotSourceCache(book,invalid,spec(),[&]{return std::string(mode)=="cancelled";});}catch(const std::exception&){refused=true;}require(refused,"source-cache preparation refuses stale, nonconverged, erroneous or cancelled calculations");}
}
void malformedCaches(){auto generated=generatePivot(source(),calculated(),spec());for(const auto* problem:{"index","width","type","nonfinite"}){auto fields=generated.definition["cache"]["fields"],records=generated.definition["cache"]["records"];if(std::string(problem)=="index")records[0][0]=99999;if(std::string(problem)=="width")records[0].erase(records[0].begin());if(std::string(problem)=="type")fields[0]["items"][0]={{"type","number"},{"value","bad"}};if(std::string(problem)=="nonfinite")fields[1]["items"][0]["value"]=std::numeric_limits<double>::infinity();bool rejected=false;try{generatePivotFromCache(source(),spec(),fields,records);}catch(const std::exception&){rejected=true;}require(rejected,"malformed, nonfinite or out-of-range native cache must be refused");}}
void fictitiousDateGrouping(){auto book=source();book["sheets"][0]["cells"][0]["input"]="Date";book["sheets"][0]["cells"][2]["input"]="60";book["sheets"][0]["cells"][4]["input"]="61";book["sheets"][0]["cells"][6]["input"]="100";auto definition=spec();definition["rows"]=Json::array({{{"field","Date"},{"group","year"}}});bool refused=false;try{generatePivot(book,calculated(),definition);}catch(const std::exception&){refused=true;}require(refused,"fictitious1900-02-29 cannot be authored as a native XML date group");}
void formattedDateCategories(){auto book=source();auto& cells=book["sheets"][0]["cells"];cells[0]["input"]="Date";for(int i=0;i<3;++i){cells[2+i*2]["input"]=std::to_string(45292+i);cells[2+i*2]["style"]={{"numberFormat","m/d/yyyy"}};}auto definition=spec();definition["rows"]=Json::array({{{"field","Date"}}});auto generation=generatePivot(book,calculated(),definition);require(generation.cells[2].at("style").at("numberFormat")=="m/d/yyyy","ungrouped date row labels retain their source date format");applyPivotTransaction(book,pivotOperations(book,generation)[0]);const auto data=pivotChartData(book,pivotChart(book,"p1","dates"));require(data.at("categories")==Json::array({"1/1/2024","1/2/2024","1/3/2024"}),"PivotChart numeric dates use the shared source-field number formatter");}
void sparseCalculationEquality(){
 auto book=source();auto& cells=book["sheets"][0]["cells"];cells=Json::array();
 const std::vector<std::vector<std::string>> rows={{"Region","Revenue","Date","Active","Code"},{"East","10%","1/1/2024","true","'00123"},{"East","=20%","45293","FALSE","Text"},{"'East","-2","1/3/2024","=TRUE",""},{"West","+1","1/4/2024","TRUE","'"}};
 for(std::size_t r=0;r<rows.size();++r)for(std::size_t c=0;c<rows[r].size();++c)cells.push_back({{"row",r+1},{"col",c+1},{"input",rows[r][c]}});
 auto definition=spec();definition["source"]["row2"]=5;definition["source"]["col2"]=5;definition["rows"]=Json::array({{{"field","Region"}},{{"field","Active"}},{{"field","Code"}}});definition["columns"]=Json::array({{{"field","Date"},{"group","month"}}});definition["values"].push_back({{"field","Code"},{"aggregate","count"},{"name","Codes"}});
 CalculationEngine engine;const auto full=engine.calculate(book);book["_tigersheetsComputedResults"]=true;const auto sparse=engine.calculate(book);
 require(sparse.at("values").size()<full.at("values").size(),"opt-in calculation emits fewer literal results");
 const auto original=generatePivot(book,full,definition),compact=generatePivot(book,sparse,definition);
 require(original.definition==compact.definition&&original.cells==compact.cells,"compact results preserve percent/date/lowercase boolean, formula, numeric text and blank pivot values");
 cells.push_back({{"row",5},{"col",6},{"input","=1/0"}});definition["source"]["col2"]=6;cells.push_back({{"row",1},{"col",6},{"input","Errors"}});
 const auto failed=engine.calculate(book);bool retainedError=false;for(const auto& value:failed.at("values"))if(value.at("row")==5&&value.at("col")==6)retainedError=value.at("type")=="error";
 require(retainedError&&!failed.at("diagnostics").empty(),"compact results retain literal/formula errors and diagnostics");bool refused=false;try{generatePivot(book,failed,definition);}catch(const std::exception&){refused=true;}require(refused,"compact calculation cannot hide a report source error");
}
int main(){try{sumReport();columnReport();typedFilters();rejectsUnqualifiedSources();dateGroups();numericDateOrder();sourceLimits();ownedRefresh();reportLookup();linkedPivotChart();nativeEngineTextTypes();cacheRendering();sourceCachePreparation();malformedCaches();fictitiousDateGrouping();formattedDateCategories();sparseCalculationEquality();std::cout<<"pivot tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
