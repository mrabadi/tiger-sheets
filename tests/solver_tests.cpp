#include "tigersheets/solver.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <filesystem>

using tigersheets::Json;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Json model() {
    return Json::parse(R"json({"revision":1,"sheets":[{"id":"m","name":"Model","cells":[
        {"row":1,"col":1,"input":"0"},{"row":1,"col":2,"input":"0"},
        {"row":1,"col":3,"input":"=3*A1+2*B1"},
        {"row":2,"col":3,"input":"=A1+B1"}
    ]}]})json");
}

void linearProgram() {
    const auto snapshot = model();
    const auto specification = Json::parse(R"json({
        "objective":{"sheet":"m","row":1,"col":3},"goal":"max","method":"linear",
        "variables":[{"sheet":"m","row":1,"col":1,"lower":0,"upper":2},
                     {"sheet":"m","row":1,"col":2,"lower":0,"upper":10}],
        "constraints":[{"sheet":"m","row":2,"col":3,"relation":"<=","value":4}],
        "maxEvaluations":200,"tolerance":0.000001
    })json");
    const auto result = tigersheets::solveWorkbook(snapshot, specification);
    require(result.at("status") == "optimal", result.dump().c_str());
    require(std::abs(result.at("objective").get<double>() - 10.0) < 1e-7, "Expected LP objective 10");
    require(result.at("feasibility").get<bool>(), "Optimal solution must pass actual workbook constraints");
    require(result.at("changingCells").size() == 2, "Solver must report changing cell inputs");
    require(snapshot == model(), "Solver must not mutate the source workbook");
}
void nonlinearProgram() {
 const auto snapshot=Json::parse(R"json({"revision":1,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"0"},{"row":1,"col":2,"input":"=(A1-3)^2"}]}]})json");
 const auto specification=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":2},"goal":"min","method":"nonlinear","variables":[{"sheet":"m","row":1,"col":1,"lower":-10,"upper":10}],"constraints":[{"sheet":"m","row":1,"col":1,"relation":">=","value":1}],"maxEvaluations":500,"tolerance":0.0000001})json");
 const auto result=tigersheets::solveWorkbook(snapshot,specification);
 require(result.at("feasibility").get<bool>(),result.dump().c_str());
 require(result.at("objective").get<double>()<1e-9,"Nonlinear Solver must minimize actual workbook formula");
 require(std::abs(std::stod(result.at("changingCells")[0].at("input").get<std::string>())-3.0)<1e-4,"Nonlinear optimum changing cell must be 3");
 require(result.at("evaluations").get<int>()<=500,"Nonlinear evaluation budget");
 require(snapshot.at("sheets")[0].at("cells")[0].at("input")=="0","Nonlinear Solver must leave source unchanged");
}

void evolutionaryProgram() {
 const auto snapshot=Json::parse(R"json({"revision":1,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"0"},{"row":1,"col":2,"input":"0"},{"row":1,"col":3,"input":"=(A1-2.4)^2+(B1-1.2)^2"},{"row":2,"col":3,"input":"=A1+B1"}]}]})json");
 const auto specification=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":3},"goal":"min","method":"evolutionary","variables":[{"sheet":"m","row":1,"col":1,"lower":0,"upper":5,"integer":true},{"sheet":"m","row":1,"col":2,"lower":0,"upper":5}],"constraints":[{"sheet":"m","row":2,"col":3,"relation":"<=","value":4}],"seed":42,"maxEvaluations":3000,"tolerance":0.00001})json");
 const auto result=tigersheets::solveWorkbook(snapshot,specification);
 require(result.at("feasibility").get<bool>(),result.dump().c_str());require(result.at("status")=="feasible","Evolutionary search must not claim a proven global optimum");
 require(result.at("objective").get<double>()<0.18,"Mixed integer nonlinear search must approach the known optimum 0.16");
 require(std::stod(result.at("changingCells")[0].at("input").get<std::string>())==2.0,"Integer changing cell must remain integral");
 require(result.at("evaluations").get<int>()<=3000,"Evolutionary evaluation budget");
 require(tigersheets::solveWorkbook(snapshot,specification)==result,"Same seed and snapshot must reproduce the search result");
 require(snapshot.at("sheets")[0].at("cells")[0].at("input")=="0","Evolutionary Solver must leave source unchanged");
}

void solverBoundaries() {
 const auto snapshot=model();
 auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":3},"goal":"value","target":7,"method":"linear","variables":[{"sheet":"m","row":1,"col":1,"lower":0,"upper":2},{"sheet":"m","row":1,"col":2,"lower":0,"upper":10}],"constraints":[{"sheet":"m","row":2,"col":3,"relation":"<=","value":4}],"maxEvaluations":1000,"tolerance":0.000001})json");
 auto result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("feasibility").get<bool>()&&std::abs(result.at("objective").get<double>()-7)<1e-6,"Linear target-value goal");
 spec["goal"]="max";spec["constraints"][0]["relation"]=">=";spec["constraints"][0]["value"]=20;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("status")=="infeasible"&&!result.at("feasibility").get<bool>()&&result.at("changingCells").empty(),"Infeasible LP must not offer changing cells");
 spec["constraints"]=Json::array();spec["variables"][0]["binary"]=true;spec["variables"][1]["upper"]=0;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("status")=="optimal"&&result.at("objective")==3,"Binary linear model uses real integrality");
 spec["variables"][0]["binary"]=false;spec["variables"][0]["integer"]=true;spec["variables"][0]["upper"]=2.8;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("status")=="optimal"&&result.at("objective")==6,"Integer upper bound rounds inward");
 auto nonlinear=snapshot;nonlinear["sheets"][0]["cells"][2]["input"]="=A1^2+B1";
 spec["variables"][0]["integer"]=false;spec["variables"][1]["upper"]=10;
 result=tigersheets::solveWorkbook(nonlinear,spec);require(result.at("status")=="error"&&result.at("message").get<std::string>().find("nonlinear")!=std::string::npos,"Linear method rejects nonlinear objective probes");
 nonlinear["sheets"][0]["cells"][2]["input"]="=1/0";
 result=tigersheets::solveWorkbook(nonlinear,spec);require(result.at("status")=="error"&&result.at("changingCells").empty(),"Formula errors cannot become a numeric Solver result");
 spec["variables"][0]["lower"]=8;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("status")=="error"&&result.at("evaluations")==0,"Invalid bounds fail before calculation");
}
void nonlinearEqualityAndIntegerSearch() {
 const auto snapshot=Json::parse(R"json({"revision":1,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"0"},{"row":1,"col":2,"input":"=(A1-3)^2"}]}]})json");
 auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":2},"goal":"min","method":"nonlinear","variables":[{"sheet":"m","row":1,"col":1,"lower":-10,"upper":10}],"constraints":[{"sheet":"m","row":1,"col":1,"relation":"=","value":2}],"maxEvaluations":500,"tolerance":0.000001})json");
 auto result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("feasibility").get<bool>()&&std::abs(result.at("objective").get<double>()-1)<1e-5,"Continuous equality constraint enforced");
 spec["variables"][0]["integer"]=true;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("status")=="error"&&result.at("message").get<std::string>().find("evolutionary")!=std::string::npos,"Continuous nonlinear backend must not silently round integer models");
 spec["method"]="evolutionary";spec["variables"][0]["lower"]=0;spec["variables"][0]["upper"]=5;spec["seed"]=42;
 result=tigersheets::solveWorkbook(snapshot,spec);require(result.at("feasibility").get<bool>()&&std::abs(result.at("objective").get<double>()-1)<1e-7,"Evolutionary equality and integer constraints enforced");
}

void limitedSearchIsNotAnAcceptedSolution(){
 const auto snapshot=Json::parse(R"json({"revision":1,"sheets":[{"id":"m","name":"Model","cells":[{"row":1,"col":1,"input":"0"},{"row":1,"col":2,"input":"=(A1-3)^2"}]}]})json");
 const auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":2},"goal":"min","method":"nonlinear","variables":[{"sheet":"m","row":1,"col":1,"lower":-10,"upper":10}],"maxEvaluations":1})json");
 const auto result=tigersheets::solveWorkbook(snapshot,spec);require(!result.at("feasibility").get<bool>()&&result.at("changingCells").empty()&&!result.value("success",false),"A merely feasible start at the evaluation limit is not a converged Solver solution and cannot offer accepted edits");
}
void settingsAndCancellation(){
 auto snapshot=model();snapshot["sheets"][0]["cells"][2]["input"]="=A1*2";
 auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":3},"goal":"min","method":"nonlinear","variables":[{"sheet":"m","row":1,"col":1,"lower":-10,"upper":10,"start":2.5}],"constraints":[],"maxEvaluations":500})json");
 const auto normalized=tigersheets::normalizeSolverSpecification(snapshot,spec);require(normalized["variables"][0]["start"]==2.5&&normalized["seed"]==42&&normalized["tolerance"]==1e-7&&tigersheets::normalizeSolverSpecification(snapshot,normalized)==normalized,"Saved Solver settings normalize explicitly and round-trip start/bounds/constraints/options");
 auto one=spec;one["maxEvaluations"]=1;const auto initial=tigersheets::solveWorkbook(snapshot,one);require(initial["objective"]==5&&!initial.at("success").get<bool>()&&initial.at("operations").empty(),"Explicit start must drive initial calculation without accepting an evaluation-limited solution");
 for(const auto* method:{"linear","nonlinear","evolutionary"}){
  spec["method"]=method;auto stopped=tigersheets::solveWorkbook(snapshot,spec,nullptr,[]{return true;});require(stopped["status"]=="cancelled"&&stopped["evaluations"]==0&&stopped["operations"].empty(),"All Solver backends honor cancellation before any calculation");
  tigersheets::CalculationContext context;context.snapshot=snapshot;context.generation=17;bool cancel=false;int calls=0;tigersheets::CalculationEngine engine;context.evaluator=[&](const Json& book){++calls;auto result=engine.calculate(book);cancel=true;return result;};const auto during=tigersheets::solveWorkbook(snapshot,spec,&context,[&]{return cancel;});require(during["status"]=="cancelled"&&calls==1&&during["operations"].empty(),"All Solver backends honor cancellation after a calculation before publishing edits");
 }
 spec["method"]="nonlinear";tigersheets::CalculationContext context;context.snapshot=snapshot;context.generation=17;bool current=true;context.generationIsCurrent=[&](std::uint64_t generation){return current&&generation==17;};tigersheets::CalculationEngine engine;context.evaluator=[&](const Json& book){auto calculated=engine.calculate(book);current=false;return calculated;};auto stale=tigersheets::solveWorkbook(snapshot,spec,&context);require(stale["status"]=="stale_context"&&stale["operations"].empty(),"A context replaced during evaluation cannot publish a stale Solver solution");
 for(const auto* problem:{"start","integer","coordinate","budget","coordinateOverflow","budgetOverflow","linearSeedOverflow"}){auto invalid=spec;if(std::string(problem)=="start")invalid["variables"][0]["start"]=99;if(std::string(problem)=="integer")invalid["variables"][0]["integer"]=true;if(std::string(problem)=="coordinate")invalid["objective"]["row"]=1.5;if(std::string(problem)=="budget")invalid["maxEvaluations"]=2.5;if(std::string(problem)=="coordinateOverflow")invalid["objective"]["row"]=4294967297LL;if(std::string(problem)=="budgetOverflow")invalid["maxEvaluations"]=4294967297LL;if(std::string(problem)=="linearSeedOverflow"){invalid["method"]="linear";invalid["seed"]=4294967295ULL;}bool refused=false;try{tigersheets::normalizeSolverSpecification(snapshot,invalid);}catch(const std::exception&){refused=true;}require(refused,"Saved Solver specifications must refuse invalid starts, fractional coordinates and fractional budgets");}
}
void circularFinancingSolver(){
 std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"fixtures/forecast-circular-financing.json");Json fixture;file>>fixture;const auto book=fixture.at("workbook");
 tigersheets::CalculationContext context;context.snapshot=book;context.generation=23;context.snapshot["links"]=Json::array({fixture.at("retainedLink")});tigersheets::CalculationEngine engine;context.evaluator=[&](const Json& candidate){return engine.calculate(candidate,[](const std::string&,const Json& args){return Json{{"status","completed"},{"value",args.at(0)}};});};
 const Json specification={{"objective",{{"sheet","m"},{"row",1},{"col",2}}},{"goal","value"},{"target",40},{"method","nonlinear"},{"variables",Json::array({{{"sheet","m"},{"row",1},{"col",1},{"start",100},{"lower",0},{"upper",150}}})},{"maxEvaluations",500},{"tolerance",1e-7}};
 const auto result=tigersheets::solveWorkbook(book,specification,&context);require(result.value("success",false)&&std::abs(std::stod(result.at("changingCells")[0].at("input").get<std::string>())-114)<1e-4&&result.at("contextGeneration")==23,"Solver circular financing must converge against frozen capex and read-only UDF rate");require(book==fixture.at("workbook"),"Circular Solver acceptance must preserve all source cells/settings");
 context.snapshot["settings"]["maxIterations"]=1;const auto failed=tigersheets::solveWorkbook(book,specification,&context);bool explained=false;for(const auto& diagnostic:failed.at("diagnostics"))explained|=diagnostic.value("code",std::string())=="iteration_not_converged";require(failed.at("status")=="calculation_failed"&&failed.at("operations").empty()&&explained,"Circular Solver refusal must retain the underlying iteration diagnostic rather than a generic optimizer failure");
}
void smallCurvatureIsNotLinear() {
 auto snapshot=model();snapshot["sheets"][0]["cells"][2]["input"]="=0.00000001*A1^2+B1";
 const auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":3},"goal":"min","method":"linear","variables":[{"sheet":"m","row":1,"col":1,"lower":-10,"upper":10},{"sheet":"m","row":1,"col":2,"lower":0,"upper":0}],"constraints":[],"maxEvaluations":200,"tolerance":0.0000001})json");
 const auto result=tigersheets::solveWorkbook(snapshot,spec);
 require(result.at("status")=="error","Global bounded probes must reject small curvature that local unit probes miss");
}
void piecewiseIncentiveCannotClaimAnOptimalLinearSolution() {
 auto snapshot=model();snapshot["sheets"][0]["cells"][2]["input"]="=A1+IF(AND(A1>=50,A1<=75),1000,0)";
 auto spec=Json::parse(R"json({"objective":{"sheet":"m","row":1,"col":3},"goal":"max","method":"linear","variables":[{"sheet":"m","row":1,"col":1,"lower":0,"upper":100}],"constraints":[],"maxEvaluations":200})json");
 const auto before=snapshot;auto result=tigersheets::solveWorkbook(snapshot,spec);
 require(result.at("status")=="error"&&!result.at("feasibility").get<bool>()&&result.at("changingCells").empty(),
     "Piecewise incentive formulas must be structurally refused rather than claiming objective100 optimal when A1=75 produces1075");
 require(snapshot==before,"Refused linear models must preserve workbook inputs");
}

void cachedReportResults(){
 auto source=model();source["sheets"][0]["cells"][2]["input"]="110";source["sheets"][0]["cells"][2]["financialModelOwner"]="comparison";source["financialModels"]=Json::array({{{"version",1},{"id","comparison"},{"kind","scenario_comparison"},{"stale",true}}});const Json spec={{"objective",{{"sheet","m"},{"row",1},{"col",3}}},{"goal","min"},{"method","linear"},{"variables",Json::array({{{"sheet","m"},{"row",1},{"col",1},{"lower",0},{"upper",10}}})}};
 auto answer=tigersheets::solveWorkbook(source,spec);require(!answer.value("success",false)&&answer.at("operations").empty(),"Solver must refuse a saved scenario-comparison literal objective that does not recalculate for trial inputs");bool refused=false;try{tigersheets::normalizeSolverSpecification(source,spec);}catch(...){refused=true;}require(refused,"saved Solver normalization must reject unsupported cached financial objectives");
 auto constraint=spec;constraint["objective"]={{"sheet","m"},{"row",2},{"col",3}};constraint["constraints"]=Json::array({{{"sheet","m"},{"row",1},{"col",3},{"relation",">="},{"value",100}}});refused=false;try{tigersheets::normalizeSolverSpecification(source,constraint);}catch(...){refused=true;}require(refused,"Solver constraints cannot consume cached generated financial results");source["sheets"][0]["cells"][2].erase("financialModelOwner");require(tigersheets::normalizeSolverSpecification(source,constraint).at("constraints").size()==1,"ordinary unowned constant constraints remain legitimate");source["sheets"][0]["cells"][2]["financialModelOwner"]="comparison";source["financialModels"][0]["kind"]="forecast";require(tigersheets::normalizeSolverSpecification(source,constraint).at("constraints").size()==1,"explicit forecast assumption constants remain readable to Solver");
}

}

int main(int argc,char** argv) {
    try {
        const std::string selected=argc>1?argv[1]:"all";
        if(selected=="all"||selected=="linear")linearProgram();
        if(selected=="all"||selected=="nonlinear")nonlinearProgram();
        if(selected=="all"||selected=="evolutionary")evolutionaryProgram();
        if(selected=="all"||selected=="boundaries"){solverBoundaries();nonlinearEqualityAndIntegerSearch();}
        if(selected=="all"||selected=="linearity")smallCurvatureIsNotLinear();
        if(selected=="all"||selected=="financing")circularFinancingSolver();
        if(selected=="all"||selected=="settings")settingsAndCancellation();
        if(selected=="all"||selected=="limits")limitedSearchIsNotAnAcceptedSolution();
        if(selected=="all"||selected=="cached")cachedReportResults();
        if(selected=="all"||selected=="affinity")piecewiseIncentiveCannotClaimAnOptimalLinearSolution();
        std::cout << "solver tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "solver test failed: " << error.what() << '\n';
        return 1;
    }
}
