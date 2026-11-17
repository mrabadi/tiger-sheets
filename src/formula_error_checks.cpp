#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/references.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
namespace tigersheets {
namespace {
using Json=nlohmann::json;
std::string_view trim(std::string_view text){while(!text.empty()&&std::isspace(static_cast<unsigned char>(text.front())))text.remove_prefix(1);while(!text.empty()&&std::isspace(static_cast<unsigned char>(text.back())))text.remove_suffix(1);return text;}
bool equal(std::string_view a,std::string_view b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});}
bool number(std::string_view text){text=trim(text);if(!text.empty()&&text.front()=='+')text.remove_prefix(1);double value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty()&&parsed.ec==std::errc()&&parsed.ptr==text.data()+text.size()&&std::isfinite(value);}
struct Numbers {
 std::map<int,std::set<int>> rows,columns;
 void add(int row,int column){rows[row].insert(column);columns[column].insert(row);}
 bool contains(int row,int column)const{const auto found=rows.find(row);return found!=rows.end()&&found->second.contains(column);}
 bool complete(const FormulaReference& range)const{
  const bool vertical=range.col1==range.col2;const auto& axes=vertical?columns:rows;const auto found=axes.find(vertical?range.col1:range.row1);if(found==axes.end())return false;
  const int first=vertical?range.row1:range.col1,last=vertical?range.row2:range.col2;return std::distance(found->second.lower_bound(first),found->second.upper_bound(last))==last-first+1;
 }
};
struct Source {const Json* sheet=nullptr;Numbers numbers;};
using Sheets=std::map<std::string,Source>;
Sheets indexBook(const Json& book,const Json* calculation=nullptr){
 Sheets result;for(const auto& sheet:book.at("sheets")){auto& source=result[sheet.at("id").get<std::string>()];source.sheet=&sheet;for(const auto& cell:sheet.at("cells"))if(number(cell.value("input",std::string())))source.numbers.add(cell.at("row"),cell.at("col"));}
 if(calculation)for(const auto& value:calculation->value("values",Json::array())){const auto found=result.find(value.value("sheet",std::string()));if(found!=result.end()&&value.contains("value")&&value.at("value").is_number()&&value.value("type",std::string())!="error"&&std::isfinite(value.at("value").get<double>()))found->second.numbers.add(value.at("row"),value.at("col"));}
 return result;
}
const Source* findSheet(const Sheets& sources,const std::string& name,const std::string& ownId){for(const auto& [id,source]:sources)if(name.empty()?id==ownId:equal(source.sheet->value("name",std::string()),name))return &source;return nullptr;}
bool ignored(const Json& sheet,int row,int col,const std::string& code){
 for(const auto& rule:sheet.value("ignoredErrors",Json::array())){const auto checks=rule.value("checks",Json::object());const auto flag=checks.find(code);if(flag==checks.end()||!flag->is_boolean()||!flag->get<bool>())continue;for(const auto& range:rule.value("ranges",Json::array()))if(row>=range.at("row1").get<int>()&&row<=range.at("row2").get<int>()&&col>=range.at("col1").get<int>()&&col<=range.at("col2").get<int>())return true;}
 return false;
}
std::optional<FormulaReference> sumRange(const std::string& formula){
 auto text=trim(formula);if(text.empty()||text.front()!='=')return std::nullopt;text=trim(text.substr(1));if(!text.empty()&&(text.front()=='+'||text.front()=='-'))text=trim(text.substr(1));if(text.size()<5||!equal(text.substr(0,3),"SUM"))return std::nullopt;text=trim(text.substr(3));if(text.empty()||text.front()!='('||text.back()!=')')return std::nullopt;const auto argument=trim(text.substr(1,text.size()-2));const auto references=formulaReferences(argument);if(references.size()!=1||references[0].start!=0||references[0].length!=argument.size())return std::nullopt;const auto& range=references[0];if((range.row1==range.row2)==(range.col1==range.col2))return std::nullopt;return range;
}
std::string externalName(const std::string& formula){const auto first=formula.find('[');if(first==std::string::npos)return {};const auto last=formula.find(']',first);if(last==std::string::npos)return {};auto name=formula.substr(first+1,last-first-1);for(std::size_t at=0;(at=name.find("''",at))!=std::string::npos;++at)name.erase(at,1);return name;}
}
bool numberStoredAsTextWarning(const Json& sheet,int row,int col,std::string_view input){
 return input.starts_with('\'')&&number(input.substr(1))&&!ignored(sheet,row,col,"numberStoredAsText");
}
Json formulaErrorChecks(const Json& snapshot,const Json& calculation){
 Json result=Json::array();const auto iteration=calculation.value("iteration",Json::object());if(calculation.value("status",std::string())!="ok"||(iteration.is_object()&&!iteration.value("converged",true)))return result;
 const auto local=indexBook(snapshot,&calculation);std::map<std::string,Sheets> external;
 if(snapshot.contains("links"))for(const auto& link:snapshot.at("links"))if(link.value("enabled",true)&&link.contains("workbook")){const auto name=link.value("name",std::string());if(!name.empty())external.emplace(name,indexBook(link.at("workbook")));}
 for(const auto& [id,own]:local)for(const auto& cell:own.sheet->at("cells")){
  const int row=cell.at("row"),col=cell.at("col");const auto formula=cell.value("input",std::string());
  if(numberStoredAsTextWarning(*own.sheet,row,col,formula))result.push_back({{"sheet",id},{"row",row},{"col",col},{"code","numberStoredAsText"}});
  if(ignored(*own.sheet,row,col,"formulaRange"))continue;const auto range=sumRange(formula);if(!range)continue;const Source* source=nullptr;
  if(range->external){const auto name=externalName(formula);for(const auto& [key,sheets]:external)if(equal(key,name)){source=findSheet(sheets,range->sheet,{});break;}}
  else source=findSheet(local,range->sheet,id);
  if(!source||!source->numbers.complete(*range))continue;
  const bool vertical=range->col1==range->col2;const auto adjacent=[&](int r,int c){return !(source->sheet==own.sheet&&r==row&&c==col)&&source->numbers.contains(r,c);};
  if((vertical&&(adjacent(range->row1-1,range->col1)||adjacent(range->row2+1,range->col1)))||(!vertical&&(adjacent(range->row1,range->col1-1)||adjacent(range->row1,range->col2+1))))result.push_back({{"sheet",id},{"row",row},{"col",col},{"code","formulaRange"}});
 }
 return result;
}
}
