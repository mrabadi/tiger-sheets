#include "tigersheets/financial_example.hpp"
#include <set>
#include <algorithm>
#include <cctype>
namespace tigersheets {
Json financialExampleOperations(const Json& book){
 std::set<std::string> sheetIds,sheetNames,modelIds,modelNames;for(const auto& sheet:book.at("sheets")){sheetIds.insert(sheet.at("id").get<std::string>());sheetNames.insert(sheet.at("name").get<std::string>());}if(book.contains("financialModels"))for(const auto& model:book.at("financialModels")){modelIds.insert(model.at("id").get<std::string>());modelNames.insert(model.at("name").get<std::string>());}
 const auto unique=[](std::set<std::string>& used,const std::string& base){const auto folded=[](std::string text){std::transform(text.begin(),text.end(),text.begin(),[](unsigned char ch){return std::tolower(ch);});return text;};const auto exists=[&](const std::string& candidate){return std::any_of(used.begin(),used.end(),[&](const std::string& value){return folded(value)==folded(candidate);});};std::string value=base;int suffix=2;while(exists(value))value=base+" "+std::to_string(suffix++);used.insert(value);return value;};
 const auto inputs=unique(sheetIds,"finance-inputs"),model=unique(sheetIds,"circular-financing"),inputName=unique(sheetNames,"Finance assumptions"),modelName=unique(sheetNames,"Circular financing");Json operations=Json::array({{{"type","add_sheet"},{"id",inputs},{"name",inputName}},{{"type","add_sheet"},{"id",model},{"name",modelName}}});
 const auto put=[&](const std::string& sheet,int row,int col,const std::string& input,const Json& style=Json::object()){Json operation={{"type","set_cell"},{"sheet",sheet},{"row",row},{"col",col},{"input",input}};if(!style.empty())operation["style"]=style;operations.push_back(std::move(operation));};
 const Json money={{"numberFormat","#,##0.00"}},heading={{"bold",true}};
 put(inputs,1,1,"Finance assumptions",heading);put(inputs,2,1,"Operating cash");put(inputs,2,2,"100",money);put(inputs,3,1,"Capital expenditure");put(inputs,3,2,"150",money);put(inputs,4,1,"Interest rate");put(inputs,4,2,"0.1",{{"numberFormat","0.00%"}});
 put(model,1,1,"Circular financing",heading);for(const auto& [row,label]:std::vector<std::pair<int,std::string>>{{2,"Operating cash"},{3,"Capital expenditure"},{4,"Interest rate"},{6,"Debt funding"},{7,"Interest expense"},{8,"Cash after interest"}})put(model,row,1,label);
 for(int row:{2,3,4})put(model,row,2,"='"+inputName+"'!B"+std::to_string(row),row==4?Json{{"numberFormat","0.00%"}}:money);
 put(model,6,2,"=MAX(0,B3-B2+B7)",money);put(model,7,2,"=B6*B4",money);put(model,8,2,"=B2-B7",money);put(model,10,1,"Debt and interest form a circular reference. Iteration must converge before analysis can be accepted.");
 auto settings=book.value("settings",Json::object());settings["iterationEnabled"]=true;settings["maxIterations"]=200;settings["maxChange"]=1e-10;operations.push_back({{"type","set_metadata"},{"key","settings"},{"value",settings}});
 for(const auto& [name,cash]:std::vector<std::pair<std::string,double>>{{"Base financing",100},{"Upside financing",120}}){Json definition={{"version",1},{"id",unique(modelIds,name)},{"name",unique(modelNames,name)},{"kind","scenario"},{"cells",Json::array({{{"sheet",inputs},{"row",2},{"col",2},{"type","number"},{"value",cash}}})}};operations.push_back({{"type","save_financial_definition"},{"definition",definition}});}
 const Json specification={{"changing",{{"sheet",inputs},{"row",2},{"col",2}}},{"output",{{"sheet",model},{"row",6},{"col",2}}},{"target",40},{"start",100},{"lower",0},{"upper",150},{"tolerance",1e-7},{"relativeTolerance",0},{"maxIterations",100},{"maxEvaluations",200}};
 operations.push_back({{"type","save_financial_definition"},{"definition",{{"version",1},{"id",unique(modelIds,"finance-goal")},{"name",unique(modelNames,"Debt target 40")},{"kind","goal_seek"},{"specification",specification}}}});return operations;
}
}
