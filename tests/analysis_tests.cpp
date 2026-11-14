#include "tigersheets/analysis.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <fstream>
#include <filesystem>
void workbookGoalSeek(){
 using Json=nlohmann::json;const Json book=Json::parse(R"json({"revision":11,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"50","style":{"numberFormat":"0.00"}},{"row":1,"col":2,"input":"=A1*1200-75000"}]}]})json");
 Json specification={{"changing",{{"sheet","m"},{"row",1},{"col",1}}},{"output",{{"sheet","m"},{"row",1},{"col",2}}},{"target",15000},{"lower",0},{"upper",100},{"maxEvaluations",30}};
 const auto before=book;auto solved=tigersheets::goalSeekWorkbook(book,specification);
 if(!solved.at("success").get<bool>()||solved.at("status")!="converged"||std::abs(solved.at("value").get<double>()-75)>1e-7||solved.at("operations").size()!=1||solved.at("revision")!=11||book!=before)throw std::runtime_error("Workbook Goal Seek must offer one isolated converged proposal and preserve input/style/formula state");
 specification["maxEvaluations"]=1;auto limited=tigersheets::goalSeekWorkbook(book,specification);if(limited.at("status")!="evaluation_limit"||limited.at("success").get<bool>()||!limited.at("operations").empty())throw std::runtime_error("Incomplete workbook Goal Seek must never offer accepted operations");
 specification["maxEvaluations"]=30;auto cancelled=tigersheets::goalSeekWorkbook(book,specification,nullptr,[]{return true;});if(cancelled.at("status")!="cancelled"||cancelled.at("evaluations")!=0||!cancelled.at("operations").empty())throw std::runtime_error("Goal Seek cancellation must perform no calculations or mutations");
 auto invalid=specification;invalid["changing"]["row"]=1.5;if(tigersheets::goalSeekWorkbook(book,invalid).at("status")!="invalid_input")throw std::runtime_error("Financial model addresses must retain integer coordinate semantics");
}
void circularFinancingWorkflow(){
 using tigersheets::Json;std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"fixtures/forecast-circular-financing.json");Json fixture;file>>fixture;const auto& book=fixture.at("workbook");const auto before=book;
 tigersheets::CalculationContext context;context.snapshot=book;context.generation=23;context.snapshot["links"]=Json::array({fixture.at("retainedLink")});bool current=true;context.generationIsCurrent=[&](std::uint64_t generation){return current&&generation==23;};tigersheets::CalculationEngine engine;int functions=0;
 context.evaluator=[&](const Json& candidate){return engine.calculate(candidate,[&](const std::string& name,const Json& args){if(name!="FinanceRate")throw std::runtime_error("Unexpected financing UDF");++functions;return Json{{"status","completed"},{"value",args.at(0)}};});};
 auto solved=tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context);
 if(!solved.at("success").get<bool>()||std::abs(solved.at("value").get<double>()-114)>1e-5||solved.at("contextGeneration")!=23||functions==0||book!=before)throw std::runtime_error("Circular debt/interest Goal Seek must use frozen linked assumptions and the read-only UDF evaluator: "+solved.dump());
 auto verify=context.snapshot;verify["sheets"][0]["cells"][0]["input"]=solved.at("changingCells")[0].at("input");const auto calculated=context.calculate(verify);for(const auto& cell:calculated.at("values"))if(cell.at("row")==2&&cell.at("col")==2&&std::abs(cell.at("value").get<double>()-4)>1e-6)throw std::runtime_error("Accepted financing interest must equal4");
 context.snapshot["settings"]["maxIterations"]=1;auto divergent=tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context);if(divergent.at("status")!="calculation_failed"||!divergent.at("operations").empty())throw std::runtime_error("Nonconverged circular financing must offer no accepted Goal Seek cells");
 context.snapshot["settings"]["maxIterations"]=200;context.snapshot["links"][0]["state"]="cached";auto cached=tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context);if(cached.at("success").get<bool>()||!cached.at("operations").empty())throw std::runtime_error("Cached financial assumptions require explicit context consent");context.allowCachedLinks=true;if(!tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context).at("success").get<bool>())throw std::runtime_error("Explicit cached-source consent must remain functional");
 current=false;if(tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context).at("status")!="stale_context")throw std::runtime_error("Stale contexts cannot execute Goal Seek");
 current=true;context.evaluator=[&](const Json& candidate){auto value=engine.calculate(candidate);current=false;return value;};const auto replaced=tigersheets::goalSeekWorkbook(book,fixture.at("specification"),&context);if(replaced.at("status")!="stale_context"||!replaced.at("operations").empty())throw std::runtime_error("A context replaced during Goal Seek cannot publish a candidate");
}
int main(){
 try{workbookGoalSeek();circularFinancingWorkflow();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}

 tigersheets::GoalSeekOptions options;options.maxEvaluations=1;int calls=0;
 auto limited=tigersheets::goalSeek([&](double x){++calls;return x*2;},10,0,options);
 if(limited.status!="evaluation_limit"||limited.evaluations!=1||calls!=1){std::cerr<<"FAILED: Goal Seek hard evaluation limit must include derivative probes\n";return 1;}
 options.maxEvaluations=200;options.lower=0;options.upper=10;
 auto bounded=tigersheets::goalSeek([](double x){return x*x;},4,0,options);
 if(bounded.status!="converged"||std::abs(bounded.value-2)>1e-7){std::cerr<<"FAILED: safeguarded Goal Seek must honor bounds while solving a flat starting derivative\n";return 1;}
 auto plateau=tigersheets::GoalSeekOptions{};plateau.lower=0;plateau.upper=250;
 const auto financed=tigersheets::goalSeek([](double cash){return std::max(0.0,(150-cash)/0.9);},40,200,plateau);
 if(financed.status!="converged"||std::abs(financed.value-114)>1e-6){std::cerr<<"FAILED: bounded Goal Seek must escape a flat no-debt starting region\n";return 1;}
 calls=0;auto stopped=tigersheets::goalSeek([&](double x){++calls;return x*2;},10,0,options,[&]{return calls==1;});
 if(stopped.status!="cancelled"||calls!=1){std::cerr<<"FAILED: cancellation after a model calculation must prevent the derivative probe\n";return 1;}
 options.tolerance=std::numeric_limits<double>::quiet_NaN();
 if(tigersheets::goalSeek([](double x){return x;},1,0,options).status!="invalid_input"){std::cerr<<"FAILED: nonfinite options must fail before evaluation\n";return 1;}

 auto result=tigersheets::goalSeek([](double price){return price*1200-75000;},15000,50);
 if(result.status!="converged"||std::abs(result.value-75)>1e-7){std::cerr<<"FAILED: Goal Seek solves model without changing original inputs\n";return 1;}
 auto bad=tigersheets::goalSeek([](double){return std::nan("");},10,1);
 if(bad.status!="invalid_result"){std::cerr<<"FAILED: non-finite model result is diagnosed\n";return 1;}
 auto table=tigersheets::sensitivityTable({10,20},{1,2,3},[](double price,double units){return price*units;});
 if(table!=std::vector<std::vector<double>>{{10,20,30},{20,40,60}}){std::cerr<<"FAILED: two variable sensitivity table evaluates all input pairs\n";return 1;}
 auto pivot=tigersheets::pivotRows(nlohmann::json::array({{{"Region","East"},{"Revenue",12}},{{"Region","West"},{"Revenue",7}},{{"Region","East"},{"Revenue",8}}}),{"Region"},"Revenue","sum");
 if(pivot["rows"].size()!=2||pivot["rows"][0]["value"]!=20||pivot["total"]!=27){std::cerr<<"FAILED: pivot groups dimensions and sums actual records\n";return 1;}
 try {
  const auto records=nlohmann::json::array({{{"Region","East"},{"Revenue",12}},{{"Region","West"},{"Revenue",7}},{{"Region","East"},{"Revenue",8}},{{"Region","East"},{"Revenue","note"}}});
  const auto average=tigersheets::pivotRows(records,{"Region"},"Revenue","average");
  if(average["rows"][0]["value"]!=10||average["total"]!=9){std::cerr<<"FAILED: Pivot average must use numeric records and weighted grand total\n";return 1;}
  const auto large=nlohmann::json::array({{{"Region","East"},{"Revenue",1e308}},{{"Region","East"},{"Revenue",1e308}}});
  const auto largeAverage=tigersheets::pivotRows(large,{"Region"},"Revenue","average");
  if(largeAverage["total"]!=1e308){std::cerr<<"FAILED: Pivot average must avoid intermediate overflow for finite inputs\n";return 1;}
  bool overflow=false;try{tigersheets::pivotRows(large,{"Region"},"Revenue","sum");}catch(const std::exception&){overflow=true;}
  if(!overflow){std::cerr<<"FAILED: Non-finite pivot totals must be explicit failures\n";return 1;}
  const auto count=tigersheets::pivotRows(records,{"Region"},"Revenue","count");
  if(count["rows"][0]["value"]!=3||count["total"]!=4){std::cerr<<"FAILED: Pivot count must count nonempty measure values\n";return 1;}
  const auto minimum=tigersheets::pivotRows(records,{"Region"},"Revenue","min");
  const auto maximum=tigersheets::pivotRows(records,{"Region"},"Revenue","max");
  if(minimum["rows"][0]["value"]!=8||minimum["total"]!=7||maximum["rows"][0]["value"]!=12||maximum["total"]!=12){std::cerr<<"FAILED: Pivot min/max must aggregate original numeric records\n";return 1;}
 } catch(const std::exception& error){std::cerr<<"FAILED: Requested pivot aggregates are unavailable: "<<error.what()<<'\n';return 1;}

 std::cout<<"analysis tests passed\n";
}
