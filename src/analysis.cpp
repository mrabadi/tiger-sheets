#include "tigersheets/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <numeric>
#include <stdexcept>
namespace tigersheets {
GoalSeekResult goalSeek(const std::function<double(double)>& model,double target,double guess,const GoalSeekOptions& options,const std::function<bool()>& cancelled){
 GoalSeekResult r{"not_converged",guess};
 const double lower=options.lower.value_or(-std::numeric_limits<double>::max()),upper=options.upper.value_or(std::numeric_limits<double>::max());
 if(!std::isfinite(target)||!std::isfinite(guess)||!std::isfinite(options.tolerance)||options.tolerance<=0||!std::isfinite(options.relativeTolerance)||options.relativeTolerance<0||options.maxIterations<1||options.maxIterations>1000000||options.maxEvaluations<1||options.maxEvaluations>1000000||(options.lower&&!std::isfinite(lower))||(options.upper&&!std::isfinite(upper))||lower>upper||guess<lower||guess>upper){r.status="invalid_input";return r;}
 const auto stopped=[&]{if(cancelled&&cancelled()){r.status="cancelled";return true;}return false;};
 std::optional<std::pair<double,double>> negative,positive;
 const auto sample=[&](double x,double& y){
  if(stopped())return false;if(r.evaluations>=options.maxEvaluations){r.status="evaluation_limit";return false;}
  if(!std::isfinite(x)){r.status="invalid_result";return false;}++r.evaluations;
  try{y=model(x);}catch(const std::exception& error){r.status="invalid_result";r.message=error.what();return false;}
  if(stopped())return false;if(!std::isfinite(y)){r.status="invalid_result";return false;}
  const auto residual=y-target;if(!std::isfinite(residual)){r.status="invalid_result";return false;}
  if(residual<0){if(!negative||!positive||std::abs(x-positive->first)<std::abs(negative->first-positive->first))negative=std::pair{x,residual};}
  else if(residual>0){if(!positive||!negative||std::abs(x-negative->first)<std::abs(positive->first-negative->first))positive=std::pair{x,residual};}
  if(std::abs(residual)<=options.tolerance+options.relativeTolerance*std::abs(target)){r.value=x;r.result=y;r.residual=residual;r.status="converged";return false;}return true;
 };
 double x=guess,y=0;if(!sample(x,y))return r;
 for(int i=0;i<options.maxIterations;++i){
  r.iterations=i+1;r.value=x;r.result=y;r.residual=y-target;
  const auto step=std::max(1e-6,std::abs(x)*1e-6);double probe=std::min(upper,x+step);if(!std::isfinite(probe)||probe==x)probe=std::max(lower,x-step);if(probe==x)return r;
  double sampled=0;if(!sample(probe,sampled))return r;const auto derivative=(sampled-y)/(probe-x);
  double next=std::numeric_limits<double>::quiet_NaN();if(std::isfinite(derivative)&&std::abs(derivative)>1e-16)next=x-(y-target)/derivative;
  if(negative&&positive){const auto left=std::min(negative->first,positive->first),right=std::max(negative->first,positive->first);if(!std::isfinite(next)||next<=left||next>=right)next=std::midpoint(left,right);}
  if(!std::isfinite(next)&&options.lower&&options.upper){next=std::midpoint(lower,upper);if(next==x)next=std::midpoint(lower,x);}
  if(!std::isfinite(next)){r.status="not_converged";r.message="The local derivative is flat; choose another starting value or supply bounds";return r;}next=std::clamp(next,lower,upper);if(next==x)return r;
  x=next;if(!sample(x,y))return r;
 }
 r.value=x;r.result=y;r.residual=y-target;r.status="iteration_limit";return r;
}
GoalSeekResult goalSeek(const std::function<double(double)>& model,double target,double guess,double tolerance,int iterations,const std::function<bool()>& cancelled){
 GoalSeekOptions options;options.tolerance=tolerance;options.maxIterations=iterations;options.maxEvaluations=iterations>0&&iterations<=500000?iterations*2:0;return goalSeek(model,target,guess,options,cancelled);
}
namespace {
Json seekAddress(const Json& value){
 if(!value.is_object()||!value.at("sheet").is_string()||value.at("sheet").get_ref<const std::string&>().empty()||!value.at("row").is_number_integer()||!value.at("col").is_number_integer())throw std::invalid_argument("Goal Seek cell addresses require a worksheet and integer coordinates");
 const auto row=value.at("row").get<std::int64_t>(),col=value.at("col").get<std::int64_t>();if(row<1||row>1048576||col<1||col>16384)throw std::invalid_argument("Goal Seek address is outside worksheet bounds");return {{"sheet",value.at("sheet")},{"row",row},{"col",col}};
}
const Json* seekCell(const Json& book,const Json& address){for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==address.at("sheet")){for(const auto& cell:sheet.at("cells"))if(cell.at("row")==address.at("row")&&cell.at("col")==address.at("col"))return &cell;return nullptr;}throw std::invalid_argument("Goal Seek worksheet was not found");}
Json& seekInput(Json& book,const Json& address){for(auto& sheet:book.at("sheets"))if(sheet.at("id")==address.at("sheet")){for(auto& cell:sheet.at("cells"))if(cell.at("row")==address.at("row")&&cell.at("col")==address.at("col"))return cell;Json cell=address;cell.erase("sheet");cell["input"]="";sheet["cells"].push_back(std::move(cell));return sheet["cells"].back();}throw std::invalid_argument("Goal Seek worksheet was not found");}
double seekNumber(const Json& value){if(!value.is_number()||!std::isfinite(value.get<double>()))throw std::invalid_argument("Goal Seek numbers must be finite");return value.get<double>();}
int seekBudget(const Json& spec,const char* name,int fallback){if(!spec.contains(name))return fallback;const auto& value=spec.at(name);if(!value.is_number_integer()||value.get<std::int64_t>()<1||value.get<std::int64_t>()>1000000)throw std::invalid_argument("Goal Seek budgets must be integers between1 and1000000");return value.get<int>();}
GoalSeekOptions seekOptions(const Json& spec){GoalSeekOptions options;options.tolerance=seekNumber(spec.value("tolerance",Json(1e-9)));options.relativeTolerance=seekNumber(spec.value("relativeTolerance",Json(0)));options.maxIterations=seekBudget(spec,"maxIterations",100);options.maxEvaluations=seekBudget(spec,"maxEvaluations",200);if(spec.contains("lower"))options.lower=seekNumber(spec.at("lower"));if(spec.contains("upper"))options.upper=seekNumber(spec.at("upper"));if(options.tolerance<=0||options.relativeTolerance<0||(options.lower&&options.upper&&*options.lower>*options.upper))throw std::invalid_argument("Goal Seek tolerance or bounds are invalid");return options;}
}
Json normalizeGoalSeekSpecification(const Json& snapshot,const Json& specification){
 const auto changing=seekAddress(specification.at("changing")),output=seekAddress(specification.at("output"));if(changing==output)throw std::invalid_argument("Goal Seek changing and output cells must differ");const auto* cell=seekCell(snapshot,changing);const auto input=cell?cell->value("input",std::string()):std::string();double initial=0;if(!input.empty()){std::size_t used=0;try{initial=std::stod(input,&used);}catch(...){throw std::invalid_argument("Goal Seek changing cell must contain a numeric constant");}if(used!=input.size()||!std::isfinite(initial))throw std::invalid_argument("Goal Seek changing cell must contain a numeric constant");}
 const auto* formula=seekCell(snapshot,output);if(!formula||!formula->value("input",std::string()).starts_with('='))throw std::invalid_argument("Goal Seek output must contain a formula");
 const auto options=seekOptions(specification);const auto start=seekNumber(specification.value("start",Json(initial)));if((options.lower&&start<*options.lower)||(options.upper&&start>*options.upper))throw std::invalid_argument("Goal Seek start lies outside the bounds");
 Json normalized={{"changing",changing},{"output",output},{"target",seekNumber(specification.at("target"))},{"start",start},{"tolerance",options.tolerance},{"relativeTolerance",options.relativeTolerance},{"maxIterations",options.maxIterations},{"maxEvaluations",options.maxEvaluations}};if(options.lower)normalized["lower"]=*options.lower;if(options.upper)normalized["upper"]=*options.upper;return normalized;
}
Json goalSeekWorkbook(const Json& snapshot,const Json& specification,const CalculationContext* context,const std::function<bool()>& cancelled){
 Json reply={{"status","invalid_input"},{"success",false},{"revision",snapshot.value("revision",Json())},{"contextGeneration",context?context->generation:0},{"operations",Json::array()},{"changingCells",Json::array()},{"diagnostics",Json::array()},{"evaluations",0},{"iterations",0},{"value",nullptr},{"result",nullptr},{"residual",nullptr}};
 try{
  const auto normalized=normalizeGoalSeekSpecification(snapshot,specification);reply["specification"]=normalized;
  if(context&&(!context->current()||context->snapshot.value("revision",Json())!=snapshot.value("revision",Json()))){reply["status"]="stale_context";throw std::runtime_error("Calculation context changed before Goal Seek");}
  CalculationEngine engine;std::string failure;Json calculationDiagnostics=Json::array();const auto& base=context?context->snapshot:snapshot;
  auto result=goalSeek([&](double value){auto candidate=base;seekInput(candidate,normalized.at("changing"))["input"]=Json(value).dump();const auto calculated=context?context->calculate(candidate):engine.calculate(candidate);calculationDiagnostics=calculated.value("diagnostics",Json::array());
   if(context&&!context->current()){failure="stale_context";throw std::runtime_error("Calculation context changed during Goal Seek");}
   if(calculated.value("status",std::string())=="cancelled"){failure="cancelled";throw std::runtime_error("Goal Seek calculation cancelled");}
   if(calculated.value("revision",Json())!=snapshot.value("revision",Json())||calculated.value("status",std::string())!="ok"||!calculated.value("converged",true)||(calculated.contains("iteration")&&!calculated.at("iteration").value("converged",true))){failure="calculation_failed";throw std::runtime_error("Goal Seek requires current, successful, converged calculations");}
   const auto& output=normalized.at("output");for(const auto& cell:calculated.at("values"))if(cell.at("sheet")==output.at("sheet")&&cell.at("row")==output.at("row")&&cell.at("col")==output.at("col")){if(cell.at("type")!="number")throw std::runtime_error("Goal Seek output is nonnumeric or contains a formula error");return seekNumber(cell.at("value"));}throw std::runtime_error("Goal Seek output was not calculated");
  },normalized.at("target"),normalized.at("start"),seekOptions(normalized),cancelled);
  if(!failure.empty())result.status=failure;if(cancelled&&cancelled())result.status="cancelled";if(context&&!context->current())result.status="stale_context";
  reply["status"]=result.status;reply["evaluations"]=result.evaluations;reply["iterations"]=result.iterations;reply["diagnostics"]=std::move(calculationDiagnostics);
  if(result.evaluations&&std::isfinite(result.value)&&std::isfinite(result.result)&&std::isfinite(result.residual)){reply["value"]=result.value;reply["result"]=result.result;reply["residual"]=result.residual;}
  if(result.status=="converged"){reply["success"]=true;auto operation=normalized.at("changing");operation["type"]="set_cell";operation["input"]=Json(result.value).dump();reply["operations"].push_back(operation);operation.erase("type");reply["changingCells"].push_back(std::move(operation));}
  else reply["diagnostics"].push_back({{"code",result.status},{"message",result.message.empty()?"Goal Seek stopped without a converged solution":result.message}});
 }catch(const std::exception& error){reply["diagnostics"].push_back({{"code",reply.at("status")},{"message",error.what()}});}
 return reply;
}

std::vector<std::vector<double>> sensitivityTable(const std::vector<double>& rows,const std::vector<double>& cols,const std::function<double(double,double)>& model){
 std::vector<std::vector<double>> result;
 for(double row:rows){auto& values=result.emplace_back();for(double col:cols)values.push_back(model(row,col));}
 return result;
}
nlohmann::json pivotRows(const nlohmann::json& records,const std::vector<std::string>& dimensions,const std::string& measure,const std::string& requested){
 using Json=nlohmann::json;
 const auto aggregate=requested=="avg"?std::string("average"):requested;
 if(aggregate!="sum"&&aggregate!="count"&&aggregate!="average"&&aggregate!="min"&&aggregate!="max")throw std::invalid_argument("Unknown pivot aggregate");
 if(!records.is_array())throw std::invalid_argument("Pivot records must be an array");
 struct Statistics {long double sum=0;double minimum=0,maximum=0;std::size_t numbers=0,nonempty=0;
  void add(const Json& value){
   if(!value.is_null()&&(!value.is_string()||!value.get_ref<const std::string&>().empty()))++nonempty;
   if(!value.is_number())return;
   const auto number=value.get<double>();if(!std::isfinite(number))throw std::invalid_argument("Pivot values must be finite");
   if(numbers==0)minimum=maximum=number;else {minimum=std::min(minimum,number);maximum=std::max(maximum,number);}
   sum+=number;++numbers;
  }
  Json value(const std::string& kind)const{
   if(kind=="count")return nonempty;
   if(numbers==0)return kind=="sum"?Json(0.0):Json();
   if(kind=="min")return minimum;
   if(kind=="max")return maximum;
   const auto result=static_cast<double>(kind=="average"?sum/static_cast<long double>(numbers):sum);
   if(!std::isfinite(result))throw std::overflow_error("Pivot aggregate exceeds the supported numeric range");
   return result;
  }
 };
 struct Group {Json keys;Statistics statistics;};
 std::map<std::string,Group> groups;Statistics total;
 for(const auto& record:records){
  if(!record.is_object())throw std::invalid_argument("Pivot record must be an object");
  Json keys=Json::array();for(const auto& dimension:dimensions)keys.push_back(record.value(dimension,Json()));
  auto& group=groups[keys.dump()];group.keys=std::move(keys);const auto value=record.value(measure,Json());group.statistics.add(value);total.add(value);
 }
 Json rows=Json::array();for(const auto& [key,group]:groups){static_cast<void>(key);rows.push_back({{"keys",group.keys},{"value",group.statistics.value(aggregate)}});}
 return {{"rows",rows},{"total",total.value(aggregate)},{"dimensions",dimensions},{"measure",measure},{"aggregate",aggregate}};
}
}
