#include "tigersheets/solver.hpp"
#include <Highs.h>
#include <nlopt.hpp>
#include <pagmo/algorithms/ihs.hpp>
#include <pagmo/population.hpp>
#include <pagmo/problem.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace tigersheets {
namespace {
struct Address {std::string sheet;int row{},col{};};
Address address(const Json& value){
 if(!value.at("row").is_number_integer()||!value.at("col").is_number_integer())throw std::invalid_argument("Solver cell coordinates must be integers");
 const auto row=value.at("row").get<std::int64_t>(),col=value.at("col").get<std::int64_t>();const auto sheet=value.at("sheet").get<std::string>();if(sheet.empty()||row<1||row>1048576||col<1||col>16384)throw std::invalid_argument("Solver cell address is invalid");return {sheet,static_cast<int>(row),static_cast<int>(col)};
}
int evaluationBudget(const Json& spec){const auto value=spec.value("maxEvaluations",Json(10000));if(!value.is_number_integer()||value.get<std::int64_t>()<1||value.get<std::int64_t>()>1000000)throw std::invalid_argument("Solver evaluation budget must be an integer between1 and1000000");return value.get<int>();}
struct Variable {Address cell;double lower{},upper{},initial{};bool integer{},binary{};};
struct Constraint {Address cell;std::string relation;double value{};};
Json& sheetAt(Json& book,const Address& a){for(auto& sheet:book.at("sheets"))if(sheet.at("id")==a.sheet)return sheet;throw std::invalid_argument("Solver worksheet was not found: "+a.sheet);}
Json& cellAt(Json& book,const Address& a){auto& cells=sheetAt(book,a).at("cells");for(auto& cell:cells)if(cell.at("row")==a.row&&cell.at("col")==a.col)return cell;cells.push_back({{"row",a.row},{"col",a.col},{"input","0"}});return cells.back();}
void recalculatingSource(const Json& book,const Address& address){
 if(!book.contains("financialModels"))return;std::string owner;
 for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==address.sheet)for(const auto& cell:sheet.at("cells"))if(cell.at("row")==address.row&&cell.at("col")==address.col){owner=cell.value("financialModelOwner",std::string());break;}
 if(owner.empty())return;for(const auto& definition:book.at("financialModels"))if(definition.value("version",Json())==1&&definition.value("id",Json())==owner&&(definition.value("kind",Json())=="scenario_comparison"||definition.value("kind",Json())=="sensitivity"))throw std::invalid_argument("Saved comparison and sensitivity result cells are snapshots, not recalculating Solver sources. Select the original model result cell instead.");
}
double finiteNumber(const Json& value){if(!value.is_number())throw std::invalid_argument("Solver bounds and targets must be numeric");const auto number=value.get<double>();if(!std::isfinite(number))throw std::invalid_argument("Solver numbers must be finite");return number;}
struct Model {
 Json snapshot;
 const CalculationContext* context;
 std::function<bool()> cancelled;
 mutable std::string failure;
 Json calculationDiagnostics=Json::array();
 CalculationEngine engine;
 Address objective;
 std::vector<Variable> variables;
 std::vector<Constraint> constraints;
 std::string goal,method;
 double tolerance{},target{};
 int maxEvaluations{},evaluations{};
 unsigned seed{};
 std::map<std::vector<double>,std::vector<double>> cache;
 Model(const Json& book,const Json& spec,const CalculationContext* prepared,const std::function<bool()>& stop):snapshot(prepared?prepared->snapshot:book),context(prepared),cancelled(stop),objective(address(spec.at("objective"))),goal(spec.value("goal","min")),method(spec.value("method","linear")),tolerance(spec.value("tolerance",1e-7)),maxEvaluations(evaluationBudget(spec)),seed(spec.value("seed",42u)) {
  check();
  recalculatingSource(snapshot,objective);
  if(spec.contains("maxEvaluations")&&!spec.at("maxEvaluations").is_number_integer())throw std::invalid_argument("Solver evaluation budget must be an integer");
  if(spec.contains("seed")&&(!spec.at("seed").is_number_integer()||finiteNumber(spec.at("seed"))<0||finiteNumber(spec.at("seed"))>std::numeric_limits<unsigned>::max()))throw std::invalid_argument("Solver seed must be an unsigned integer");
  if(context&&(!context->current()||snapshot.value("revision",Json())!=book.value("revision",Json())))throw std::invalid_argument("Solver calculation context is stale");
  if(method=="linear"&&static_cast<std::uint64_t>(seed)>static_cast<std::uint64_t>(std::numeric_limits<HighsInt>::max()))throw std::invalid_argument("Linear Solver seed exceeds the supported signed integer range");
  if(goal!="min"&&goal!="max"&&goal!="value")throw std::invalid_argument("Solver goal must be min, max or value");
  if(method!="linear"&&method!="nonlinear"&&method!="evolutionary")throw std::invalid_argument("Unknown Solver method");
  if(!std::isfinite(tolerance)||tolerance<=0||tolerance>1||maxEvaluations<1||maxEvaluations>1000000)throw std::invalid_argument("Invalid Solver tolerance or evaluation budget");
  if(goal=="value")target=finiteNumber(spec.at("target"));
  if(!spec.at("variables").is_array()||spec["variables"].empty()||spec["variables"].size()>200)throw std::invalid_argument("Solver requires 1–200 changing cells");
  std::set<std::tuple<std::string,int,int>> seen;
  for(const auto& entry:spec["variables"]){
   Variable variable;variable.cell=address(entry);
   if(!seen.emplace(variable.cell.sheet,variable.cell.row,variable.cell.col).second)throw std::invalid_argument("Solver changing cells must be unique");
   if(method!="linear"&&(!entry.contains("lower")||!entry.contains("upper")))throw std::invalid_argument("Nonlinear and evolutionary Solver require finite lower and upper bounds");
   variable.lower=finiteNumber(entry.value("lower",Json(-1e20)));variable.upper=finiteNumber(entry.value("upper",Json(1e20)));
   const bool binary=entry.value("binary",false);variable.binary=binary;variable.integer=entry.value("integer",false)||binary;
   if(binary){variable.lower=std::max(0.0,variable.lower);variable.upper=std::min(1.0,variable.upper);}
   if(variable.integer){variable.lower=std::ceil(variable.lower);variable.upper=std::floor(variable.upper);}
   if(variable.lower>variable.upper)throw std::invalid_argument("Solver variable bounds are reversed or contain no integer");
   const auto input=cellAt(snapshot,variable.cell).value("input",std::string("0"));
   if(!input.empty()){
    std::size_t used=0;try{variable.initial=std::stod(input,&used);}catch(...){throw std::invalid_argument("Solver changing cells must contain numeric constants");}
    if(used!=input.size()||!std::isfinite(variable.initial))throw std::invalid_argument("Solver changing cells must contain numeric constants");
   }
   if(entry.contains("start")){variable.initial=finiteNumber(entry.at("start"));if(variable.initial<variable.lower||variable.initial>variable.upper||(variable.integer&&std::abs(variable.initial-std::round(variable.initial))>1e-12))throw std::invalid_argument("Solver start must satisfy its bounds and integer type");}
   variable.initial=std::clamp(variable.initial,variable.lower,variable.upper);if(variable.integer)variable.initial=std::round(variable.initial);
   variables.push_back(variable);
  }
  const auto entries=spec.value("constraints",Json::array());if(!entries.is_array()||entries.size()>1000)throw std::invalid_argument("Solver supports at most 1000 constraints");
  for(const auto& entry:entries){Constraint constraint{address(entry),entry.at("relation").get<std::string>(),finiteNumber(entry.at("value"))};if(constraint.relation!="<="&&constraint.relation!=">="&&constraint.relation!="=")throw std::invalid_argument("Unknown Solver constraint relation");recalculatingSource(snapshot,constraint.cell);constraints.push_back(constraint);}
 }
 void check()const{if(cancelled&&cancelled()){failure="cancelled";throw std::runtime_error("Solver cancelled");}if(context&&!context->current()){failure="stale_context";throw std::runtime_error("Solver calculation context changed");}}
 Json specification()const{Json spec={{"objective",{{"sheet",objective.sheet},{"row",objective.row},{"col",objective.col}}},{"goal",goal},{"method",method},{"tolerance",tolerance},{"maxEvaluations",maxEvaluations},{"seed",seed},{"variables",Json::array()},{"constraints",Json::array()}};if(goal=="value")spec["target"]=target;for(const auto& v:variables)spec["variables"].push_back({{"sheet",v.cell.sheet},{"row",v.cell.row},{"col",v.cell.col},{"lower",v.lower},{"upper",v.upper},{"start",v.initial},{"integer",v.integer},{"binary",v.binary}});for(const auto& c:constraints)spec["constraints"].push_back({{"sheet",c.cell.sheet},{"row",c.cell.row},{"col",c.cell.col},{"relation",c.relation},{"value",c.value}});return spec;}
 std::vector<double> initial()const{std::vector<double> values;for(const auto& variable:variables)values.push_back(variable.initial);return values;}
 const std::vector<double>& evaluate(const std::vector<double>& x){
  check();const auto found=cache.find(x);if(found!=cache.end())return found->second;
  if(evaluations>=maxEvaluations){failure="limit";throw std::runtime_error("Solver evaluation budget exhausted");}
  Json candidate=snapshot;for(std::size_t i=0;i<x.size();++i){if(!std::isfinite(x[i]))throw std::runtime_error("Solver generated a non-finite variable");cellAt(candidate,variables[i].cell)["input"]=Json(x[i]).dump();}
  ++evaluations;const auto calculated=context?context->calculate(candidate):engine.calculate(candidate);
  calculationDiagnostics=calculated.value("diagnostics",Json::array());
  check();if(calculated.value("status",std::string())=="cancelled"){failure="cancelled";throw std::runtime_error("Solver calculation cancelled");}
  if(calculated.value("revision",Json())!=snapshot.value("revision",Json())||calculated.value("status",std::string())!="ok"||!calculated.value("converged",true)||(calculated.contains("iteration")&&!calculated["iteration"].value("converged",true))){failure="calculation_failed";throw std::runtime_error("Solver requires a current, successful, converged workbook calculation");}
  const auto number=[&](const Address& a){for(const auto& cell:calculated.at("values"))if(cell.at("sheet")==a.sheet&&cell.at("row")==a.row&&cell.at("col")==a.col){if(cell.at("type")!="number"||!cell.at("value").is_number())throw std::runtime_error("Solver objective or constraint contains a formula error or nonnumeric value");return finiteNumber(cell.at("value"));}throw std::runtime_error("Solver objective or constraint cell was not calculated");};
  std::vector<double> values{number(objective)};for(const auto& constraint:constraints)values.push_back(number(constraint.cell));
  return cache.emplace(x,std::move(values)).first->second;
 }
 bool feasible(const std::vector<double>& x,const std::vector<double>& values)const{
  for(std::size_t i=0;i<x.size();++i){const auto& v=variables[i];if(x[i]<v.lower-tolerance||x[i]>v.upper+tolerance||(v.integer&&std::abs(x[i]-std::round(x[i]))>tolerance))return false;}
  for(std::size_t i=0;i<constraints.size();++i){const auto& c=constraints[i];const auto difference=values[i+1]-c.value;const auto epsilon=tolerance*(1+std::abs(c.value));if((c.relation=="<="&&difference>epsilon)||(c.relation==">="&&difference< -epsilon)||(c.relation=="="&&std::abs(difference)>epsilon))return false;}
  return goal!="value"||std::abs(values[0]-target)<=tolerance*(1+std::abs(target));
 }
 Json result(const std::string& status,std::vector<double> x){
  Json changes=Json::array();if(x.empty())return {{"status",status},{"success",false},{"objective",nullptr},{"feasibility",false},{"changingCells",changes},{"evaluations",evaluations}};
  for(std::size_t i=0;i<x.size();++i)if(variables[i].integer)x[i]=std::round(x[i]);
  const auto values=evaluate(x);const bool valid=feasible(x,values),accepted=valid&&(status=="optimal"||status=="converged"||status=="feasible");
  if(accepted)for(std::size_t i=0;i<x.size();++i)changes.push_back({{"sheet",variables[i].cell.sheet},{"row",variables[i].cell.row},{"col",variables[i].cell.col},{"input",Json(x[i]).dump()}});
  return {{"status",valid?status:(status=="limit"||status=="roundoff_limit"?status:"infeasible")},{"success",accepted},{"objective",values[0]},{"feasibility",accepted},{"candidateFeasibility",valid},{"changingCells",std::move(changes)},{"evaluations",evaluations}};
 }
};
Json linear(Model& model){
 Json variables=Json::array(),outputs=Json::array();
 for(const auto& variable:model.variables)variables.push_back({{"sheet",variable.cell.sheet},{"row",variable.cell.row},{"col",variable.cell.col}});
 outputs.push_back({{"sheet",model.objective.sheet},{"row",model.objective.row},{"col",model.objective.col}});
 for(const auto& constraint:model.constraints)outputs.push_back({{"sheet",constraint.cell.sheet},{"row",constraint.cell.row},{"col",constraint.cell.col}});
 model.check();const auto qualification=model.engine.qualifyAffine(model.snapshot,variables,outputs);
 model.check();if(qualification.value("status",std::string())!="affine")throw std::invalid_argument("Linear Solver cannot qualify a nonlinear or unsupported model: "+qualification.value("diagnostics",Json::array()).dump());
 const auto baseline=model.initial();const auto values=model.evaluate(baseline);const auto n=baseline.size(),expressions=values.size();std::vector<std::vector<double>> coefficients(expressions,std::vector<double>(n));std::vector<double> steps(n);
 for(std::size_t j=0;j<n;++j){
  const auto& variable=model.variables[j];if(variable.lower==variable.upper)continue;
  auto step=std::min(1.0,variable.upper-baseline[j]);if(step<1e-8)step=-std::min(1.0,baseline[j]-variable.lower);if(step==0)continue;steps[j]=step;
  auto point=baseline;point[j]+=step;const auto sampled=model.evaluate(point);
  for(std::size_t e=0;e<expressions;++e)coefficients[e][j]=(sampled[e]-values[e])/step;
  point[j]=baseline[j]+step*0.5;const auto halfway=model.evaluate(point);
  for(std::size_t e=0;e<expressions;++e)if(std::abs(halfway[e]-values[e]-coefficients[e][j]*step*0.5)>model.tolerance*(1+std::abs(halfway[e])))throw std::invalid_argument("Linear Solver detected a nonlinear objective or constraint");
 }
 const auto checkLinearity=[&](const std::vector<double>& point){const auto actual=model.evaluate(point);for(std::size_t e=0;e<expressions;++e){double predicted=values[e];for(std::size_t j=0;j<n;++j)predicted+=coefficients[e][j]*(point[j]-baseline[j]);if(!std::isfinite(predicted)||std::abs(actual[e]-predicted)>model.tolerance*(1+std::abs(actual[e])))throw std::invalid_argument("Linear Solver detected nonlinear terms across the declared variable bounds");}};
 for(double fraction:{0.37,0.79}){
  auto local=baseline,bounded=baseline;
  for(std::size_t j=0;j<n;++j){local[j]+=steps[j]*fraction;bounded[j]=(1-fraction)*model.variables[j].lower+fraction*model.variables[j].upper;}
  checkLinearity(local);checkLinearity(bounded);
 }
 std::vector<double> offsets=values;for(std::size_t e=0;e<expressions;++e)for(std::size_t j=0;j<n;++j)offsets[e]-=coefficients[e][j]*baseline[j];
 HighsLp lp;lp.num_col_=static_cast<HighsInt>(n);lp.num_row_=static_cast<HighsInt>(model.constraints.size()+(model.goal=="value"?1:0));lp.sense_=model.goal=="max"?ObjSense::kMaximize:ObjSense::kMinimize;lp.offset_=model.goal=="value"?0:offsets[0];lp.col_cost_=model.goal=="value"?std::vector<double>(n,0):coefficients[0];
 for(const auto& v:model.variables){lp.col_lower_.push_back(v.lower);lp.col_upper_.push_back(v.upper);lp.integrality_.push_back(v.integer?HighsVarType::kInteger:HighsVarType::kContinuous);}
 lp.a_matrix_.format_=MatrixFormat::kRowwise;lp.a_matrix_.start_={0};
 const auto row=[&](std::size_t e,double lower,double upper){lp.row_lower_.push_back(lower);lp.row_upper_.push_back(upper);for(std::size_t j=0;j<n;++j)if(coefficients[e][j]!=0){lp.a_matrix_.index_.push_back(static_cast<HighsInt>(j));lp.a_matrix_.value_.push_back(coefficients[e][j]);}lp.a_matrix_.start_.push_back(static_cast<HighsInt>(lp.a_matrix_.value_.size()));};
 for(std::size_t i=0;i<model.constraints.size();++i){const auto& c=model.constraints[i];const auto rhs=c.value-offsets[i+1];row(i+1,c.relation=="<="?-kHighsInf:rhs,c.relation==">="?kHighsInf:rhs);}
 if(model.goal=="value")row(0,model.target-offsets[0],model.target-offsets[0]);
 Highs highs;highs.setOptionValue("output_flag",false);highs.setOptionValue("threads",1);highs.setOptionValue("random_seed",static_cast<HighsInt>(model.seed));highs.setOptionValue("simplex_iteration_limit",static_cast<HighsInt>(model.maxEvaluations));highs.setOptionValue("mip_max_nodes",static_cast<HighsInt>(model.maxEvaluations));
 highs.setCallback([&](int,const std::string&,const HighsCallbackOutput*,HighsCallbackInput* input,void*){if(input&&((model.cancelled&&model.cancelled())||(model.context&&!model.context->current())))input->user_interrupt=true;},nullptr);
 for(const auto type:{HighsCallbackType::kCallbackSimplexInterrupt,HighsCallbackType::kCallbackIpmInterrupt,HighsCallbackType::kCallbackMipInterrupt})highs.startCallback(type);
 model.check();
 if(highs.passModel(lp)==HighsStatus::kError||highs.run()==HighsStatus::kError)throw std::runtime_error("HiGHS could not solve this workbook model");
 model.check();const auto status=highs.getModelStatus();if(status==HighsModelStatus::kInfeasible)return model.result("infeasible",{});if(status==HighsModelStatus::kUnbounded||status==HighsModelStatus::kUnboundedOrInfeasible)return model.result("unbounded",{});
 const auto& solution=highs.getSolution();if(!solution.value_valid)return model.result("no_solution",{});
 return model.result(status==HighsModelStatus::kOptimal?"optimal":"limit",solution.col_value);
}
double nonlinearObjective(const std::vector<double>& x,std::vector<double>&,void* pointer){
 auto& model=*static_cast<Model*>(pointer);const auto value=model.evaluate(x)[0];return model.goal=="value"?(value-model.target)*(value-model.target):value;
}
struct NonlinearConstraint {Model* model;std::size_t index;};
double nonlinearConstraint(const std::vector<double>& x,std::vector<double>&,void* pointer){
 const auto& item=*static_cast<NonlinearConstraint*>(pointer);const auto& constraint=item.model->constraints[item.index];
 const auto difference=item.model->evaluate(x)[item.index+1]-constraint.value;return constraint.relation==">="?-difference:difference;
}
Json nonlinear(Model& model){
 for(const auto& variable:model.variables)if(variable.integer)throw std::invalid_argument("Integer nonlinear models require the evolutionary Solver method");
 nlopt::opt optimizer(nlopt::LN_COBYLA,static_cast<unsigned>(model.variables.size()));std::vector<double> lower,upper,steps;
 for(const auto& variable:model.variables){lower.push_back(variable.lower);upper.push_back(variable.upper);steps.push_back(std::max(1e-4,std::min(1.0,(variable.upper-variable.lower)*0.1)));}
 optimizer.set_lower_bounds(lower);optimizer.set_upper_bounds(upper);optimizer.set_initial_step(steps);
 if(model.goal=="max")optimizer.set_max_objective(nonlinearObjective,&model);else optimizer.set_min_objective(nonlinearObjective,&model);
 std::vector<NonlinearConstraint> data;data.reserve(model.constraints.size());
 for(std::size_t i=0;i<model.constraints.size();++i){data.push_back({&model,i});const auto& constraint=model.constraints[i];const auto epsilon=model.tolerance*(1+std::abs(constraint.value));if(constraint.relation=="=")optimizer.add_equality_constraint(nonlinearConstraint,&data.back(),epsilon);else optimizer.add_inequality_constraint(nonlinearConstraint,&data.back(),epsilon);}
 optimizer.set_xtol_rel(model.tolerance);optimizer.set_ftol_abs(model.tolerance*model.tolerance);optimizer.set_maxeval(model.maxEvaluations);
 auto x=model.initial();double optimum=0;std::string status="converged";
 try{const auto result=optimizer.optimize(x,optimum);if(result==nlopt::MAXEVAL_REACHED||result==nlopt::MAXTIME_REACHED)status="limit";}catch(const nlopt::roundoff_limited&){status="roundoff_limit";}
 return model.result(status,std::move(x));
}

struct WorkbookProblem {
 Model* model=nullptr;
 std::vector<std::size_t> order;
 std::vector<double> natural(const pagmo::vector_double& x)const{std::vector<double> values(x.size());for(std::size_t i=0;i<order.size();++i)values[order[i]]=x[i];return values;}
 pagmo::vector_double fitness(const pagmo::vector_double& x)const{
  const auto& values=model->evaluate(natural(x));const auto objective=model->goal=="value"?(values[0]-model->target)*(values[0]-model->target):model->goal=="max"?-values[0]:values[0];
  pagmo::vector_double result{objective};
  for(const auto equality:{true,false})for(std::size_t i=0;i<model->constraints.size();++i){const auto& c=model->constraints[i];if((c.relation=="=")!=equality)continue;const auto difference=values[i+1]-c.value;result.push_back(c.relation==">="?-difference:difference);}
  return result;
 }
 std::pair<pagmo::vector_double,pagmo::vector_double> get_bounds()const{pagmo::vector_double lower,upper;for(const auto index:order){lower.push_back(model->variables[index].lower);upper.push_back(model->variables[index].upper);}return {lower,upper};}
 pagmo::vector_double::size_type get_nix()const{return static_cast<pagmo::vector_double::size_type>(std::count_if(model->variables.begin(),model->variables.end(),[](const auto& v){return v.integer;}));}
 pagmo::vector_double::size_type get_nec()const{return static_cast<pagmo::vector_double::size_type>(std::count_if(model->constraints.begin(),model->constraints.end(),[](const auto& c){return c.relation=="=";}));}
 pagmo::vector_double::size_type get_nic()const{return model->constraints.size()-get_nec();}
 pagmo::thread_safety get_thread_safety()const{return pagmo::thread_safety::none;}
};
Json evolutionary(Model& model){
 if(model.maxEvaluations<5)throw std::invalid_argument("Evolutionary Solver requires at least five evaluations");
 std::vector<std::size_t> order;for(const auto integer:{false,true})for(std::size_t i=0;i<model.variables.size();++i)if(model.variables[i].integer==integer)order.push_back(i);
 WorkbookProblem workbook{&model,order};pagmo::problem problem{workbook};pagmo::vector_double tolerance;
 for(const auto equality:{true,false})for(const auto& constraint:model.constraints)if((constraint.relation=="=")==equality)tolerance.push_back(model.tolerance*(1+std::abs(constraint.value)));
 problem.set_c_tol(tolerance);
 const auto populationSize=static_cast<unsigned>(std::min(40,model.maxEvaluations/3));pagmo::population population{problem,populationSize,model.seed};
 pagmo::vector_double initial;for(const auto index:order)initial.push_back(model.variables[index].initial);population.set_x(0,initial);
 const auto generations=static_cast<unsigned>(model.maxEvaluations-static_cast<int>(populationSize)-2);
 pagmo::ihs algorithm{generations,0.9,0.35,0.99,1e-6,1.0,model.seed};population=algorithm.evolve(std::move(population));
 return model.result("feasible",workbook.natural(population.get_x()[population.best_idx()]));
}

}
Json normalizeSolverSpecification(const Json& snapshot,const Json& specification,const CalculationContext* context){Model model(snapshot,specification,context,{});return model.specification();}
Json solveWorkbook(const Json& snapshot,const Json& specification,const CalculationContext* context,const std::function<bool()>& cancelled){
 std::unique_ptr<Model> model;
 const auto decorate=[&](Json result){
  result["revision"]=snapshot.value("revision",Json());result["contextGeneration"]=context?context->generation:0;result["operations"]=Json::array();result["diagnostics"]=model?model->calculationDiagnostics:Json::array();if(model)result["specification"]=model->specification();
  if(!result.contains("success"))result["success"]=false;
  if(result.value("success",false))for(const auto& cell:result.at("changingCells")){auto operation=cell;operation["type"]="set_cell";result["operations"].push_back(std::move(operation));}
  else result["diagnostics"].push_back({{"code",result.at("status")},{"message",result.value("status",std::string())=="calculation_failed"?std::string("Solver requires current, successful, converged source calculations"):result.value("message",std::string("Solver stopped without an accepted solution"))}});
  return result;
 };
 try{
  if(context&&(!context->current()||context->snapshot.value("revision",Json())!=snapshot.value("revision",Json())))return decorate({{"status","stale_context"},{"feasibility",false},{"changingCells",Json::array()},{"objective",nullptr},{"evaluations",0}});
  if(cancelled&&cancelled())return decorate({{"status","cancelled"},{"feasibility",false},{"changingCells",Json::array()},{"objective",nullptr},{"evaluations",0}});
  model=std::make_unique<Model>(snapshot,specification,context,cancelled);Json result;
  if(model->method=="linear")result=linear(*model);else if(model->method=="nonlinear")result=nonlinear(*model);else if(model->method=="evolutionary")result=evolutionary(*model);else throw std::invalid_argument("Solver method is not implemented");
  model->check();return decorate(std::move(result));
 }catch(const std::exception& error){return decorate({{"status",cancelled&&cancelled()?"cancelled":context&&!context->current()?"stale_context":model&&!model->failure.empty()?model->failure:"error"},{"message",error.what()},{"feasibility",false},{"changingCells",Json::array()},{"objective",nullptr},{"evaluations",model?model->evaluations:0}});}
}
}
