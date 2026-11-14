#include "tigersheets/worksheet_validation.hpp"
#include "tigersheets/formatting.hpp"
#include "tigersheets/references.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>
namespace tigersheets {
namespace {
std::string lower(std::string text){for(auto& ch:text)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');return text;}
const Json& worksheet(const Json& book,const std::string& id){for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==id)return sheet;throw std::runtime_error("Validation worksheet was not found");}
struct Source {const Json* sheet;FormulaReference range;};
Source source(const Json& book,const std::string& id,const std::string& formula){
 const auto& own=worksheet(book,id);const auto references=formulaReferences(formula);if(formula.empty()||formula.front()!='='||references.size()!=1||references[0].start!=1||references[0].length!=formula.size()-1||references[0].external)throw std::runtime_error("List source must be a worksheet cell range, such as =Input!$B$3:$B$7");
 const auto& range=references[0];if((range.row1!=range.row2&&range.col1!=range.col2)||std::max(range.row2-range.row1,range.col2-range.col1)>=10000)throw std::runtime_error("List source must be one row or column with at most 10,000 cells");
 if(range.sheet.empty())return {&own,range};for(const auto& sheet:book.at("sheets"))if(lower(sheet.at("name").get<std::string>())==lower(range.sheet))return {&sheet,range};throw std::runtime_error("List source worksheet was not found");
}
int index(const Json& object,const char* key,int maximum){const auto& value=object.at(key);if(!value.is_number_integer()||(value.is_number_unsigned()&&value.get<std::uint64_t>()>static_cast<std::uint64_t>(maximum)))throw std::runtime_error(std::string("Validation ")+key+" is out of bounds");const auto number=value.get<std::int64_t>();if(number<1||number>maximum)throw std::runtime_error(std::string("Validation ")+key+" is out of bounds");return static_cast<int>(number);}
void targetRange(const Json& range){if(!range.is_object())throw std::runtime_error("Validation range must be an object");const auto r1=index(range,"row1",MaxRows),r2=index(range,"row2",MaxRows),c1=index(range,"col1",MaxColumns),c2=index(range,"col2",MaxColumns);if(r2<r1||c2<c1)throw std::runtime_error("Validation target range is reversed");}
Json literal(const std::string& input){if(input.empty())return nullptr;if(input.front()=='\'')return input.substr(1);const auto upper=lower(input);if(upper=="true")return true;if(upper=="false")return false;double number=0;const bool percent=input.back()=='%';const auto* end=input.data()+input.size()-(percent?1:0);const auto parsed=std::from_chars(input.data(),end,number);if(parsed.ec==std::errc()&&parsed.ptr==end&&std::isfinite(number))return percent?number/100.0:number;return input;}
std::string optionInput(const Json& value){if(value.is_null())return {};if(value.is_boolean())return value.get<bool>()?"TRUE":"FALSE";if(value.is_number()){char buffer[128];const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),value.get<double>());if(converted.ec!=std::errc())throw std::runtime_error("Cannot represent validation numeric option");return std::string(buffer,converted.ptr);}return "'"+value.get<std::string>();}
}
Json makeWorksheetListValidation(const Json& book,const std::string& sheetId,const Json& range,const std::string& formula1,bool allowBlank){targetRange(range);source(book,sheetId,formula1);return {{"type","list"},{"range",range},{"formula1",formula1},{"allowBlank",allowBlank}};}
void validateWorksheetValidations(const Json& book,const Json& sheet){
 if(!sheet.contains("dataValidations"))return;const auto& rules=sheet.at("dataValidations");if(!rules.is_array()||rules.size()>10000)throw std::runtime_error("Worksheet data validations must be an array of at most 10,000 rules");
 for(std::size_t i=0;i<rules.size();++i){const auto& rule=rules[i];if(!rule.is_object()||rule.value("type",std::string())!="list"||!rule.at("formula1").is_string()||!rule.at("allowBlank").is_boolean())throw std::runtime_error("Data validation must be a list with a text source and boolean allowBlank");makeWorksheetListValidation(book,sheet.at("id").get<std::string>(),rule.at("range"),rule.at("formula1").get<std::string>(),rule.at("allowBlank").get<bool>());
  const auto& a=rule.at("range");for(std::size_t j=0;j<i;++j){const auto& b=rules[j].at("range");if(a.at("row1")<=b.at("row2")&&a.at("row2")>=b.at("row1")&&a.at("col1")<=b.at("col2")&&a.at("col2")>=b.at("col1"))throw std::runtime_error("Data validation target ranges cannot overlap");}
 }
}
const Json* worksheetListValidationAt(const Json& sheet,int row,int col){if(sheet.contains("dataValidations"))for(const auto& rule:sheet.at("dataValidations")){const auto& range=rule.at("range");if(row>=range.at("row1")&&row<=range.at("row2")&&col>=range.at("col1")&&col<=range.at("col2"))return &rule;}return nullptr;}
Json worksheetValidationOptions(const Json& book,const Json& calculated,const std::string& sheetId,const Json& validation){
 const auto resolved=source(book,sheetId,validation.at("formula1").get<std::string>());const auto& sheet=*resolved.sheet;const auto& range=resolved.range;const auto sourceId=sheet.at("id").get<std::string>();using Key=std::pair<int,int>;std::map<Key,const Json*> cells,values;
 const auto inside=[&](int row,int col){return row>=range.row1&&row<=range.row2&&col>=range.col1&&col<=range.col2;};for(const auto& cell:sheet.at("cells"))if(inside(cell.at("row"),cell.at("col")))cells[{cell.at("row"),cell.at("col")}]=&cell;
 const bool current=calculated.is_object()&&calculated.value("revision",Json())==book.at("revision")&&calculated.value("status",std::string())=="ok";if(current&&calculated.contains("values"))for(const auto& value:calculated.at("values"))if(value.at("sheet")==sourceId&&inside(value.at("row"),value.at("col")))values[{value.at("row"),value.at("col")}]=&value;
 Json options=Json::array();const bool date1904=book.value("settings",Json::object()).value("date1904",false);for(int row=range.row1;row<=range.row2;++row)for(int col=range.col1;col<=range.col2;++col){const Key key{row,col};const Json* cell=cells.contains(key)?cells.at(key):nullptr;const auto input=cell?cell->at("input").get<std::string>():std::string();Json value;std::string type;
  if(values.contains(key)){const auto& result=*values.at(key);type=result.at("type").get<std::string>();if(type=="error")throw std::runtime_error("List source contains a calculation error");value=result.at("value");}
  else{if(input.starts_with('='))throw std::runtime_error("Calculate the workbook before using formula-based list options");value=literal(input);type=value.is_null()?"blank":value.is_number()?"number":value.is_boolean()?"boolean":"string";}
  if(value.is_number()&&!std::isfinite(value.get<double>()))throw std::runtime_error("List source contains a non-finite numeric value");const auto style=cell?cell->value("style",Json::object()):Json::object();options.push_back({{"label",formattedValue(value,style,date1904)},{"input",optionInput(value)},{"type",type},{"value",value}});
 }return options;
}
}
