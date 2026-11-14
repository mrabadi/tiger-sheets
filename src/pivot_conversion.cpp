#include "tigersheets/pivot_conversion.hpp"
#include <set>
#include <stdexcept>
namespace tigersheets {
namespace {
void exactFields(const Json& value,const std::set<std::string>& keys){
 if(!value.is_object())throw std::runtime_error("Unsupported static summary metadata");
 for(const auto& [key,item]:value.items())if(!keys.contains(key))throw std::runtime_error("Unsupported static summary metadata field: "+key);
 for(const auto& key:keys)if(!value.contains(key))throw std::runtime_error("Incomplete static summary metadata: "+key);
}
std::string lower(std::string text){for(auto& ch:text)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');return text;}
int coordinate(const Json& value,int maximum){
 if(!value.is_number_integer())throw std::runtime_error("Static summary coordinates must be integers");
 if(value.is_number_unsigned()){const auto n=value.get<std::uint64_t>();if(n<1||n>static_cast<std::uint64_t>(maximum))throw std::runtime_error("Static summary coordinate is out of bounds");return static_cast<int>(n);}
 const auto n=value.get<std::int64_t>();if(n<1||n>maximum)throw std::runtime_error("Static summary coordinate is out of bounds");return static_cast<int>(n);
}
void sheetName(const std::string& name){
 if(name.empty()||name.size()>124||name.find_first_of("[]:*?/\\")!=std::string::npos||name.front()=='\''||name.back()=='\'')throw std::runtime_error("Invalid conversion worksheet name");
 std::size_t count=0;for(unsigned char ch:name){if(ch<32||ch==127)throw std::runtime_error("Invalid conversion worksheet name");count+=(ch&0xc0)!=0x80;}
 if(count>31)throw std::runtime_error("Conversion worksheet names are limited to 31 characters");
}
}
Json staticPivotConversionDefinition(const Json& book,const std::string& legacyId,
 const std::string& newSheetId,const std::string& newPivotId,const std::string& newPivotName){
 if(legacyId.empty()||newSheetId.empty()||newSheetId.size()>128||newPivotId.empty()||newPivotId.size()>128||newPivotId==legacyId||newPivotName.empty()||newPivotName.size()>255)throw std::runtime_error("Conversion requires fresh worksheet/report identities and a report name");
 for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==newSheetId)throw std::runtime_error("Conversion requires a new worksheet identity");
 if(!book.contains("pivots")||!book.at("pivots").is_array())throw std::runtime_error("Static summary was not found");
 const Json* selected=nullptr;
 for(const auto& pivot:book.at("pivots")){
  if(pivot.value("id",std::string())==newPivotId||pivot.value("name",std::string())==newPivotName)throw std::runtime_error("Conversion report identity or name already exists");
  if(pivot.value("id",std::string())==legacyId){if(selected)throw std::runtime_error("Static summary identity is ambiguous");selected=&pivot;}
 }
 if(!selected)throw std::runtime_error("Static summary was not found");
 exactFields(*selected,{"id","staticSummary","source","rowField","valueField","aggregate","output"});
 if(selected->at("staticSummary")!=true)throw std::runtime_error("Only explicit supported static summaries can be converted");
 const auto& source=selected->at("source");const auto& oldOutput=selected->at("output");
 exactFields(source,{"sheet","row1","col1","row2","col2"});exactFields(oldOutput,{"sheet","row","col"});
 const int r1=coordinate(source.at("row1"),1048576),r2=coordinate(source.at("row2"),1048576),c1=coordinate(source.at("col1"),16384),c2=coordinate(source.at("col2"),16384);
 if(r2<=r1||c2<c1||r2-r1>100000||static_cast<std::uint64_t>(r2-r1)*(c2-c1+1)>1000000)throw std::runtime_error("Static summary source exceeds native report bounds");
 coordinate(oldOutput.at("row"),1048576);coordinate(oldOutput.at("col"),16384);
 bool sourceSheet=false,oldSheet=false;for(const auto& sheet:book.at("sheets")){sourceSheet|=sheet.at("id")==source.at("sheet");oldSheet|=sheet.at("id")==oldOutput.at("sheet");}
 if(!sourceSheet||!oldSheet)throw std::runtime_error("Static summary source or output worksheet was not found");
 const auto rowField=selected->at("rowField").get<std::string>(),valueField=selected->at("valueField").get<std::string>(),aggregate=selected->at("aggregate").get<std::string>();
 if(rowField.empty()||valueField.empty()||!std::set<std::string>{"sum","count","average","min","max"}.contains(aggregate))throw std::runtime_error("Unsupported static summary fields or aggregate");
 return {{"id",newPivotId},{"name",newPivotName},{"source",source},{"rows",Json::array({{{"field",rowField}}})},{"columns",Json::array()},{"filters",Json::array()},{"values",Json::array({{{"field",valueField},{"aggregate",aggregate},{"name",aggregate+" of "+valueField},{"numberFormat","General"}}})},{"rowGrandTotals",true},{"columnGrandTotals",true},{"output",{{"sheet",newSheetId},{"row1",1},{"col1",1}}}};
}
Json convertStaticPivotOperations(const Json& book,const Json& calculated,
 const std::string& legacyId,const std::string& newSheetId,const std::string& newSheetName,
 const std::string& newPivotId,const std::string& newPivotName,
 std::uint64_t context,const std::function<bool()>& cancelled){
 if(cancelled&&cancelled())throw std::runtime_error("Static summary conversion cancelled");
 auto definition=staticPivotConversionDefinition(book,legacyId,newSheetId,newPivotId,newPivotName);sheetName(newSheetName);
 for(const auto& sheet:book.at("sheets"))if(lower(sheet.at("name").get<std::string>())==lower(newSheetName))throw std::runtime_error("Conversion requires a new worksheet name");
 Json definitions=Json::array();for(const auto& pivot:book.at("pivots"))if(pivot.at("id")!=legacyId)definitions.push_back(pivot);
 auto candidate=book;candidate["sheets"].push_back({{"id",newSheetId},{"name",newSheetName},{"cells",Json::array()}});candidate["pivots"]=definitions;
 auto native=pivotOperations(candidate,generatePivot(candidate,calculated,definition,context,cancelled));
 Json operations=Json::array({{{"type","add_sheet"},{"id",newSheetId},{"name",newSheetName}},{{"type","set_metadata"},{"key","pivots"},{"value",std::move(definitions)}}});
 for(auto& operation:native)operations.push_back(std::move(operation));
 return operations;
}
}
