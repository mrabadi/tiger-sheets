#include "tigersheets/financial_workflows.hpp"
#include "tigersheets/financial_models.hpp"
#include "tigersheets/analysis.hpp"
#include "tigersheets/solver.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include "tigersheets/financial_example.hpp"
#include <cmath>
#include <stdexcept>
namespace tigersheets {
namespace {
void current(const Json& book,const CalculationContext& context,const std::function<bool()>& cancelled){if(cancelled&&cancelled())throw std::runtime_error("Financial workflow cancelled");if(!context.current()||context.snapshot.value("revision",Json())!=book.at("revision"))throw std::runtime_error("Financial workflow calculation context is stale");}
bool report(const std::string& kind){return kind=="forecast"||kind=="scenario_comparison"||kind=="sensitivity";}
void inputOwnership(const Json& book,const Json& address){auto operation=address;operation["type"]="set_cell";operation["input"]="0";validatePivotEdit(book,operation);validateFinancialModelEdit(book,operation);}
Json provenance(const CalculationContext& context,const Json& analysis){Json links=Json::array();for(const auto& link:context.snapshot.value("links",Json::array()))links.push_back({{"name",link.value("name",std::string())},{"path",link.value("originalPath",link.value("path",std::string()))},{"state",link.value("state",std::string("unavailable"))}});return {{"allowCachedLinks",context.allowCachedLinks},{"links",std::move(links)},{"diagnostics",analysis.value("diagnostics",Json::array())}};}
}
Json executeFinancialWorkflow(const Json& book,const Json& definition,const std::string& action,const CalculationContext& context,const std::function<bool()>& cancelled){
 Json reply={{"revision",book.at("revision")},{"contextGeneration",context.generation},{"definition",definition},{"analysis",{{"status","invalid_input"},{"success",false}}},{"operations",Json::array()}};
 try{
  current(book,context,cancelled);if(!definition.is_object())throw std::runtime_error("Financial workflow definition must be an object");auto normalized=definition;
  if(action=="delete"||action=="apply"||action=="apply_scenario"){const auto id=normalized.at("id").get<std::string>();bool found=false;for(const auto& saved:book.value("financialModels",Json::array()))if(saved.value("id",std::string())==id){normalized=saved;found=true;break;}if(!found)throw std::runtime_error("Saved financial definition was not found");}
  const auto kind=action=="example"?std::string("example"):normalized.at("kind").get<std::string>();Json operations=Json::array(),analysis={{"status","saved"},{"success",true}};
  if(action=="settings"){
   if(kind!="iteration"||!normalized.at("settings").is_object()||normalized.at("settings").empty())throw std::runtime_error("Iteration settings must be a nonempty object");
   auto settings=book.value("settings",Json::object());
   for(const auto& [key,value]:normalized.at("settings").items()){
    if(key=="iterationEnabled"){if(!value.is_boolean())throw std::runtime_error("Iteration enabled must be a boolean");}
    else if(key=="maxIterations"){if(!value.is_number_integer()||value.get<std::int64_t>()<1||value.get<std::int64_t>()>100000)throw std::runtime_error("Maximum iterations must be an integer between1 and100000");}
    else if(key=="maxChange"){if(!value.is_number()||!std::isfinite(value.get<double>())||value.get<double>()<0)throw std::runtime_error("Maximum change must be finite and nonnegative");}
    else throw std::runtime_error("Unsupported iteration setting: "+key);
    settings[key]=value;
   }
   if(settings!=book.value("settings",Json::object()))operations.push_back({{"type","set_metadata"},{"key","settings"},{"value",settings}});analysis["status"]=operations.empty()?"unchanged":"settings";
  }
  else if(action=="example"){operations=financialExampleOperations(book);analysis["status"]="example";}
  else if(action=="delete")operations=deleteFinancialModelOperations(book,normalized.at("id"));
  else if(action=="apply"||action=="apply_scenario")operations=applyFinancialScenarioOperations(book,normalized.at("id"));
  else if(action=="save"||action=="run"){
   if(report(kind)){auto generated=generateFinancialModel(book,normalized,context,cancelled);operations=financialModelOperations(book,generated);normalized=std::move(generated.definition);analysis["status"]=operations.empty()?"unchanged":"generated";analysis["provenance"]=normalized.at("provenance");}
   else if(kind=="goal_seek"||kind=="solver"){
    normalized["specification"]=kind=="goal_seek"?normalizeGoalSeekSpecification(book,normalized.at("specification")):normalizeSolverSpecification(book,normalized.at("specification"),&context);
    if(kind=="goal_seek")inputOwnership(book,normalized.at("specification").at("changing"));else for(const auto& variable:normalized.at("specification").at("variables"))inputOwnership(book,variable);
    if(action=="run"){analysis=kind=="goal_seek"?goalSeekWorkbook(book,normalized.at("specification"),&context,cancelled):solveWorkbook(book,normalized.at("specification"),&context,cancelled);if(analysis.value("success",false)){analysis["provenance"]=provenance(context,analysis);if(analysis.contains("specification"))normalized["specification"]=analysis.at("specification");operations=saveFinancialDefinitionOperations(book,normalized);for(const auto& operation:analysis.at("operations"))operations.push_back(operation);}}
    else operations=saveFinancialDefinitionOperations(book,normalized);
   }else if(kind=="scenario"||kind=="assumption_set"){
    operations=saveFinancialDefinitionOperations(book,normalized);normalized["version"]=1;if(action=="run"){operations.push_back({{"type","apply_financial_scenario"},{"id",normalized.at("id")}});analysis["status"]="applied";}
   }else throw std::runtime_error("Unsupported financial workflow kind");
  }else throw std::runtime_error("Unsupported financial workflow action");
  current(book,context,cancelled);if(!analysis.value("success",false))operations=Json::array();normalized["version"]=1;reply["definition"]=std::move(normalized);reply["analysis"]=std::move(analysis);reply["operations"]=std::move(operations);if(action=="delete")reply["analysis"]["status"]="deleted";if(action=="apply"||action=="apply_scenario")reply["analysis"]["status"]="applied";
 }catch(const std::exception& error){const auto status=cancelled&&cancelled()?"cancelled":!context.current()||context.snapshot.value("revision",Json())!=book.at("revision")?"stale_context":"invalid_input";reply["analysis"]={{"status",status},{"success",false},{"message",error.what()},{"diagnostics",Json::array({{{"code",status},{"message",error.what()}}})}};reply["operations"]=Json::array();}
 return reply;
}
}
