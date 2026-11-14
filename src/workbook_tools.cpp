#include "tigersheets/workbook_tools.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/financial_workflows.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace tigersheets {
namespace {
constexpr int maxRows=1048576,maxColumns=16384,maxMutations=10000;
Json object(Json properties,Json required=Json::array()){
 return {{"type","object"},{"properties",std::move(properties)},{"required",std::move(required)},{"additionalProperties",false}};
}
Json integer(int minimum,int maximum){return {{"type","integer"},{"minimum",minimum},{"maximum",maximum}};}
Json textSchema(int maximum=32767){return {{"type","string"},{"maxLength",maximum}};}
Json identifier(){return {{"type","string"},{"minLength",1},{"maxLength",128}};}
Json rangeSchema(){return object({{"row1",integer(1,maxRows)},{"col1",integer(1,maxColumns)},{"row2",integer(1,maxRows)},{"col2",integer(1,maxColumns)}},{"row1","col1","row2","col2"});}
Json styleSchema(){
 return object({{"bold",{{"type","boolean"}}},{"italic",{{"type","boolean"}}},{"underline",{{"type","boolean"}}},
  {"fontFamily",textSchema(256)},{"fontSize",{{"type","number"},{"minimum",1},{"maximum",400}}},
  {"color",textSchema(64)},{"fill",textSchema(64)},{"alignment",{{"type","string"},{"enum",{"left","center","right"}}}},
  {"numberFormat",textSchema(4096)},{"note",textSchema()}});
}
Json chartSchema(){
 auto range=rangeSchema();range["description"]="The first row contains series headings; the first column contains categories. At least 2 rows and 2 columns; at most 10000 data rows and 100 series.";
 return object({{"id",identifier()},{"sheet",identifier()},{"type",{{"type","string"},{"enum",{"line","column","bar","area","scatter","pie","doughnut"}}}},
  {"title",textSchema(4096)},{"range",range},{"anchor",object({{"row",integer(1,maxRows)},{"col",integer(1,maxColumns)},
   {"width",integer(320,8192)},{"height",integer(230,8192)},{"offsetX",integer(0,1000000)},{"offsetY",integer(0,1000000)}})}},
  {"id","sheet","type","title","range"});
}
Json pivotSchema(){
 auto range=rangeSchema();range["properties"]["sheet"]=identifier();range["required"].push_back("sheet");
 auto axis=object({{"field",textSchema(256)},{"group",{{"type","string"},{"enum",{"year","quarter","month"}}}}},{"field"});
 return object({{"id",identifier()},{"name",identifier()},{"source",range},
  {"rows",{{"type","array"},{"items",axis}}},{"columns",{{"type","array"},{"items",axis}}},
  {"filters",{{"type","array"},{"items",object({{"field",textSchema(256)},{"all",{{"type","boolean"}}},{"items",{{"type","array"},{"items",object({{"type",{{"type","string"},{"enum",{"string","number","boolean","blank"}}}},{"value",{{"type",{"string","number","boolean","null"}}}}},{"type","value"})}}}},{"field","all"})}}},
  {"values",{{"type","array"},{"minItems",1},{"maxItems",4},{"items",object({{"field",textSchema(256)},{"aggregate",{{"type","string"},{"enum",{"sum","count","average","min","max"}}}},{"name",textSchema(256)},{"numberFormat",textSchema(4096)}},{"field","aggregate"})}}},
  {"rowGrandTotals",{{"type","boolean"}}},{"columnGrandTotals",{{"type","boolean"}}},
  {"output",object({{"sheet",identifier()},{"row1",integer(1,maxRows)},{"col1",integer(1,maxColumns)}},{"sheet","row1","col1"})}}, {"id","name","source","values","output"});
}
Json financialAddressSchema(){return object({{"sheet",identifier()},{"row",integer(1,maxRows)},{"col",integer(1,maxColumns)}},{"sheet","row","col"});}
Json numericalSpecificationSchema(bool solver){
 const auto number=Json{{"type","number"}};const auto address=financialAddressSchema();
 if(!solver)return object({{"output",address},{"changing",address},{"target",number},{"start",number},{"lower",number},{"upper",number},{"tolerance",{{"type","number"},{"exclusiveMinimum",0}}},{"relativeTolerance",{{"type","number"},{"minimum",0}}},{"maxIterations",integer(1,1000000)},{"maxEvaluations",integer(1,1000000)}},{"output","changing","target"});
 auto variable=address;for(const auto* key:{"lower","upper","start"})variable["properties"][key]=number;for(const auto* key:{"integer","binary"})variable["properties"][key]={{"type","boolean"}};
 auto constraint=address;constraint["properties"]["relation"]={{"type","string"},{"enum",{"<=",">=","="}}};constraint["properties"]["value"]=number;constraint["required"].push_back("relation");constraint["required"].push_back("value");
 return object({{"objective",address},{"goal",{{"type","string"},{"enum",{"min","max","value"}}}},{"target",number},{"method",{{"type","string"},{"enum",{"linear","nonlinear","evolutionary"}}}},{"variables",{{"type","array"},{"minItems",1},{"maxItems",200},{"items",variable}}},{"constraints",{{"type","array"},{"maxItems",1000},{"items",constraint}}},{"tolerance",{{"type","number"},{"exclusiveMinimum",0},{"maximum",1}}},{"maxEvaluations",integer(1,1000000)},{"seed",{{"type","integer"},{"minimum",0},{"maximum",4294967295ULL}}}},{"objective","variables"});
}
Json financialDefinitionSchema(bool reports=false){
 const auto branch=[](const char* kind,Json fields,Json required){fields["version"]={{"type","integer"},{"const",1}};fields["id"]=identifier();fields["name"]={{"type","string"},{"minLength",1},{"maxLength",64}};fields["kind"]={{"type","string"},{"const",kind}};for(const auto* field:{"version","id","name","kind"})required.push_back(field);return object(std::move(fields),std::move(required));};
 auto cell=financialAddressSchema();cell["properties"]["type"]={{"type","string"},{"enum",{"number","string","boolean","blank"}}};cell["properties"]["value"]={{"type",{"number","string","boolean","null"}}};cell["required"].push_back("type");cell["required"].push_back("value");const auto cells=Json{{"type","array"},{"minItems",1},{"maxItems",1000},{"items",cell}};
 auto branches=Json::array({branch("assumption_set",{{"cells",cells}},{"cells"}),branch("scenario",{{"cells",cells}},{"cells"}),branch("goal_seek",{{"specification",numericalSpecificationSchema(false)}},{"specification"}),branch("solver",{{"specification",numericalSpecificationSchema(true)}},{"specification"})});
 if(reports){
  const auto address=financialAddressSchema(),number=Json{{"type","number"}},output=object({{"sheet",identifier()},{"row1",integer(1,maxRows)},{"col1",integer(1,maxColumns)}},{"sheet","row1","col1"});
  auto result=address;result["properties"]["label"]={{"type","string"},{"minLength",1},{"maxLength",64}};result["properties"]["numberFormat"]=textSchema(256);result["required"].push_back("label");
  auto changing=address;changing["properties"]["label"]=textSchema(64);changing["properties"]["numberFormat"]=textSchema(256);changing["properties"]["values"]={{"type","array"},{"minItems",1},{"maxItems",100},{"items",number}};changing["required"].push_back("values");
  const auto source=Json{{"oneOf",Json::array({number,address})}};
  branches.push_back(branch("forecast",{{"periods",integer(1,120)},{"startPeriod",integer(1,9999)},{"drivers",{{"type","array"},{"minItems",1},{"maxItems",32},{"items",object({{"name",{{"type","string"},{"minLength",1},{"maxLength",64}}},{"base",source},{"growth",source},{"numberFormat",textSchema(256)}},{"name","base","growth"})}}},{"assumptionSetId",identifier()},{"output",output}},{"periods","drivers","output"}));
  branches.push_back(branch("scenario_comparison",{{"scenarioIds",{{"type","array"},{"minItems",1},{"maxItems",32},{"uniqueItems",true},{"items",identifier()}}},{"outputs",{{"type","array"},{"minItems",1},{"maxItems",32},{"items",result}}},{"includeBaseline",{{"type","boolean"}}},{"output",output}},{"scenarioIds","outputs","output"}));
  branches.push_back(branch("sensitivity",{{"inputs",{{"type","array"},{"minItems",1},{"maxItems",2},{"items",changing}}},{"result",result},{"output",output}},{"inputs","result","output"}));
 }
 return {{"oneOf",std::move(branches)}};
}
Json operationSchemas(){
 const auto operation=[](const char* type,Json properties,Json required,const char* description){
  properties["type"]={{"type","string"},{"const",type}};required.push_back("type");auto schema=object(std::move(properties),std::move(required));schema["description"]=description;return schema;
 };
 auto schemas=Json::array({
  operation("set_cell",{{"sheet",identifier()},{"row",integer(1,maxRows)},{"col",integer(1,maxColumns)},{"input",textSchema()},{"style",styleSchema()},{"replaceStyle",{{"type","boolean"}}}},
   {"sheet","row","col","input"},"Set one Excel input string. Formulas begin with =; a leading apostrophe forces literal text. Optional style merges unless replaceStyle is true."),
  operation("set_range",{{"sheet",identifier()},{"row",integer(1,maxRows)},{"col",integer(1,maxColumns)},
   {"values",{{"type","array"},{"minItems",1},{"maxItems",maxMutations},{"items",{{"type","array"},{"minItems",1},{"maxItems",maxMutations},{"items",{{"type",{"string","number","boolean","null"}}}}}}}}},
   {"sheet","row","col","values"},"Write a nonempty rectangular matrix from the 1-based top-left cell. Text uses Excel input syntax, numbers are decimal, booleans TRUE/FALSE and null clears input. Maximum 10000 cells across the complete preview."),
  operation("set_style",{{"sheet",identifier()},{"row",integer(1,maxRows)},{"col",integer(1,maxColumns)},{"style",styleSchema()},{"replace",{{"type","boolean"}}}},
   {"sheet","row","col","style"},"Style one cell, merging with its current style unless replace is true. Colors use #RRGGBB; numberFormat uses Excel format syntax."),
  operation("add_sheet",{{"id",identifier()},{"name",{{"type","string"},{"minLength",1},{"maxLength",31}}}},
   {"id","name"},"Create a worksheet with a new stable id. Later operations in this preview can address that id."),
  operation("rename_sheet",{{"sheet",identifier()},{"name",{{"type","string"},{"minLength",1},{"maxLength",31}}}},
   {"sheet","name"},"Rename an existing worksheet; supported local formula references update automatically."),
  operation("clear_range",{{"sheet",identifier()},{"row1",integer(1,maxRows)},{"col1",integer(1,maxColumns)},{"row2",integer(1,maxRows)},{"col2",integer(1,maxColumns)}},
   {"sheet","row1","col1","row2","col2"},"Clear inputs in a bounded inclusive rectangle while retaining formatting; at most 10000 addressed cells per preview."),
  operation("add_chart",{{"chart",chartSchema()}},{"chart"},"Append an editable native chart with a unique id while preserving every existing chart. Provide a table with categories in its first column and series headings in its first row."),
  operation("set_metadata",{{"key",{{"type","string"},{"enum",{"charts","pivots","scenarios","links","settings","vba","names"}}}},
   {"value",{{"description","Replace the complete named metadata value. Prefer add_chart for insertion. charts is an array of the chart schema; names is an object mapping names to reference strings; vba is an object with modules [{name,source}], existing project metadata and user enablement are retained, and enabling macros is unavailable. Other keys preserve their native workbook JSON format."},{"type",{"object","array"}}}}},
   {"key","value"},"Replace one supported workbook metadata field. Read its existing value first. Unrelated fields remain preserved. Macro enablement is a desktop action; VBA source proposals cannot enable macros.")
 });
 schemas.push_back(operation("create_pivot",{{"definition",pivotSchema()}},{"definition"},"Create a real native PivotTable from a local rectangular source. Headers must be unique text. Cache and generated output are owned; use typed report operations to edit. Up to100000 records/1million data cells,4measures and250000 output cells."));
 auto changes=pivotSchema();changes["required"]=Json::array();changes["properties"].erase("id");
 schemas.push_back(operation("update_pivot",{{"id",identifier()},{"changes",changes}},{"id","changes"},"Update editable fields, filters, measures or source of an existing native pivot; recompute its cache, owned output and linked charts in the same proposal."));
 schemas.push_back(operation("refresh_pivot",{{"id",identifier()}},{"id"},"Recalculate an existing native pivot from the proposed source, replacing its cache and generated output together. An unchanged refresh produces no edit."));
 schemas.push_back(operation("move_pivot",{{"id",identifier()},{"output",pivotSchema()["properties"]["output"]}},{"id","output"},"Relocate a native pivot using its retained cache and preserving stale state. Generated cells and linked charts move together; supported dependent formulas and defined names follow the move."));
 schemas.push_back(operation("delete_pivot",{{"id",identifier()}},{"id"},"Delete an editable native pivot, its generated output and its linked PivotCharts together. Source data is retained."));
 schemas.push_back(operation("create_pivot_chart",{{"pivotId",identifier()},{"id",identifier()},{"chartType",{{"type","string"},{"enum",{"line","column","bar"}}}},{"title",textSchema(4096)}},{"pivotId","id","chartType"},"Create a genuine native PivotChart linked to a proposed/existing pivot; series follow its fields and filters and exclude grand totals. Maximum1000categories/16series."));
 auto chartChanges=object({{"chartType",{{"type","string"},{"enum",{"line","column","bar"}}}},{"title",textSchema(4096)},{"anchor",chartSchema()["properties"]["anchor"]}});
 schemas.push_back(operation("update_pivot_chart",{{"id",identifier()},{"changes",chartChanges}},{"id","changes"},"Update the family, title or position of an existing editable PivotChart, retaining its owning report."));
 schemas.push_back(operation("delete_pivot_chart",{{"id",identifier()}},{"id"},"Delete an editable PivotChart while retaining its source PivotTable."));
 schemas.push_back(operation("save_financial_model",{{"definition",financialDefinitionSchema()}},{"definition"},"Save a validated assumption set, scenario, Goal Seek or Solver setup in the isolated proposal. Saving never applies inputs or accepts a numerical solution."));
 auto run=operation("run_financial_model",{{"id",identifier()},{"definition",financialDefinitionSchema(true)}},{},"Run exactly one new editable definition or saved model id using the frozen calculation context. Builds a forecast/comparison/sensitivity report, applies saved inputs, or offers only a qualified Goal Seek/Solver solution. The result remains an isolated proposal for user review.");run["oneOf"]=Json::array({Json{{"required",{"id"}}},Json{{"required",{"definition"}}}});schemas.push_back(std::move(run));
 schemas.push_back(operation("apply_financial_scenario",{{"id",identifier()}},{"id"},"Apply a saved assumption set or scenario to its validated constant input cells in the proposal, preserving formulas and generated report ownership."));
 schemas.push_back(operation("delete_financial_model",{{"id",identifier()}},{"id"},"Delete an editable saved model and its generated owned output together. Dependent models must be removed first; source model inputs are retained."));
 return schemas;
}
Json financialExample(std::uint64_t revision,const std::string& sheet){
 return {{"revision",revision},{"operations",Json::array({
  {{"type","set_range"},{"sheet",sheet},{"row",1},{"col",1},{"values",Json::array({Json::array({"Year","Revenue"}),Json::array({"2025A",12000000}),Json::array({"2026F","=B2*(1+12%)"}),Json::array({"2027F","=B3*(1+10%)"})})}},
  {{"type","add_chart"},{"chart",{{"id","revenue-forecast-example"},{"sheet",sheet},{"type","line"},{"title","Fictitious company revenue forecast"},
   {"range",{{"row1",1},{"col1",1},{"row2",4},{"col2",2}}},{"anchor",{{"row",6},{"col",1},{"width",560},{"height",300}}}}}}
 })}};
}
std::string input(const Json& value){
 if(value.is_string()){auto result=value.get<std::string>();if(result.size()>32767)throw std::runtime_error("Cell input exceeds 32767 bytes");return result;}
 if(value.is_null())return "";
 if(value.is_boolean())return value.get<bool>()?"TRUE":"FALSE";
 if(value.is_number()){if(value.is_number_float()&&!std::isfinite(value.get<double>()))throw std::runtime_error("Cell numbers must be finite");return value.dump();}
 throw std::runtime_error("Cell values must be text, numbers, booleans or null");
}
int boundedInteger(const Json& value,int minimum,int maximum,const std::string& field){
 if(!value.is_number_integer())throw std::runtime_error(field+" must be an integer");
 if(value.is_number_unsigned()){
  const auto number=value.get<std::uint64_t>();if(number<static_cast<std::uint64_t>(minimum)||number>static_cast<std::uint64_t>(maximum))throw std::runtime_error(field+" is out of bounds");return static_cast<int>(number);
 }
 const auto number=value.get<std::int64_t>();if(number<minimum||number>maximum)throw std::runtime_error(field+" is out of bounds");return static_cast<int>(number);
}
std::uint64_t revision(const Json& value){
 if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()):value.get<std::int64_t>()<0))throw std::runtime_error("Revision must be a nonnegative 64-bit integer");
 return value.get<std::uint64_t>();
}
std::string boundedText(const Json& value,std::size_t minimum,std::size_t maximum,const std::string& field){
 if(!value.is_string())throw std::runtime_error(field+" must be text");
 const auto text=value.get<std::string>();if(text.size()<minimum||text.size()>maximum)throw std::runtime_error(field+" has invalid length");return text;
}
void requireSheet(const Json& book,const Json& id){
 boundedText(id,1,128,"sheet");
 if(std::none_of(book.at("sheets").begin(),book.at("sheets").end(),[&](const auto& sheet){return sheet.at("id")==id;}))throw std::runtime_error("Worksheet id was not found; read the stable worksheet identities");
}
void knownFields(const Json& value,std::initializer_list<const char*> names){
 if(!value.is_object())throw std::runtime_error("Expected an object");
 for(const auto& [key,unused]:value.items()){static_cast<void>(unused);if(std::none_of(names.begin(),names.end(),[&](const char* name){return key==name;}))throw std::runtime_error("Unsupported field: "+key);}
}
Json normalizedChart(Json chart,const Json& book){
 knownFields(chart,{"id","sheet","type","title","range","anchor"});
 boundedText(chart.at("id"),1,128,"chart.id");requireSheet(book,chart.at("sheet"));boundedText(chart.at("title"),0,4096,"chart.title");
 const auto type=boundedText(chart.at("type"),1,32,"chart.type");
 const std::vector<std::string> supported{"line","column","bar","area","scatter","pie","doughnut"};
 if(std::find(supported.begin(),supported.end(),type)==supported.end())throw std::runtime_error("Chart type is not supported by the native renderer");
 const auto& range=chart.at("range");knownFields(range,{"row1","col1","row2","col2"});
 const auto r1=boundedInteger(range.at("row1"),1,maxRows,"chart.range.row1"),r2=boundedInteger(range.at("row2"),1,maxRows,"chart.range.row2"),
  c1=boundedInteger(range.at("col1"),1,maxColumns,"chart.range.col1"),c2=boundedInteger(range.at("col2"),1,maxColumns,"chart.range.col2");
 if(r2<=r1||c2<=c1||r2-r1>10000||c2-c1>100)throw std::runtime_error("Chart data needs 2 or more rows and columns, up to 10000 data rows and 100 series");
 if(!chart.contains("anchor"))chart["anchor"]=Json::object();
 auto& anchor=chart["anchor"];knownFields(anchor,{"row","col","width","height","offsetX","offsetY"});
 for(const auto& [key,defaultValue,minimum,maximum]:std::vector<std::tuple<std::string,int,int,int>>{{"row",2,1,maxRows},{"col",6,1,maxColumns},{"width",560,320,8192},{"height",300,230,8192},{"offsetX",0,0,1000000},{"offsetY",0,0,1000000}}){
  if(!anchor.contains(key))anchor[key]=defaultValue;boundedInteger(anchor.at(key),minimum,maximum,"chart.anchor."+key);
 }
 return chart;
}
constexpr std::size_t metadataLimit=262144;
const std::vector<std::string> metadataKeys{"charts","pivots","scenarios","links","settings","vba","names"};
void validateStyle(const Json& style){
 knownFields(style,{"bold","italic","underline","fontFamily","fontSize","color","fill","alignment","numberFormat","note"});
 for(const auto* key:{"bold","italic","underline"})if(style.contains(key)&&!style.at(key).is_boolean())throw std::runtime_error(std::string(key)+" must be boolean");
 for(const auto* key:{"fontFamily","color","fill","alignment","numberFormat","note"})if(style.contains(key))boundedText(style.at(key),0,std::string(key)=="note"?32767:4096,key);
 if(style.contains("alignment")&&style.at("alignment")!="left"&&style.at("alignment")!="center"&&style.at("alignment")!="right")throw std::runtime_error("Invalid cell alignment");
 if(style.contains("fontSize")&&(!style.at("fontSize").is_number()||!std::isfinite(style.at("fontSize").get<double>())||style.at("fontSize").get<double>()<1||style.at("fontSize").get<double>()>400))throw std::runtime_error("fontSize must be between 1 and 400");
}
void validateNativeOperation(Json& op,const Json& book){
 const auto type=boundedText(op.at("type"),1,64,"type");
 if(type=="set_cell"||type=="set_style"){
  if(type=="set_cell")knownFields(op,{"type","sheet","row","col","input","style","replaceStyle"});
  else knownFields(op,{"type","sheet","row","col","style","replace"});
  requireSheet(book,op.at("sheet"));boundedInteger(op.at("row"),1,maxRows,"row");boundedInteger(op.at("col"),1,maxColumns,"col");
  if(type=="set_cell"){if(!op.at("input").is_string())throw std::runtime_error("set_cell input must be an Excel input string");input(op.at("input"));}
  if(type=="set_style"||op.contains("style"))validateStyle(op.at("style"));
  for(const auto* key:{"replaceStyle","replace"})if(op.contains(key)&&!op.at(key).is_boolean())throw std::runtime_error(std::string(key)+" must be boolean");
 }else if(type=="add_sheet"){
  knownFields(op,{"type","id","name"});boundedText(op.at("id"),1,128,"id");boundedText(op.at("name"),1,124,"name");
 }else if(type=="rename_sheet"){
  knownFields(op,{"type","sheet","name"});requireSheet(book,op.at("sheet"));boundedText(op.at("name"),1,124,"name");
 }else if(type=="clear_range"){
  knownFields(op,{"type","sheet","row1","col1","row2","col2"});requireSheet(book,op.at("sheet"));
  const auto r1=boundedInteger(op.at("row1"),1,maxRows,"row1"),r2=boundedInteger(op.at("row2"),1,maxRows,"row2"),c1=boundedInteger(op.at("col1"),1,maxColumns,"col1"),c2=boundedInteger(op.at("col2"),1,maxColumns,"col2");
  if(r2<r1||c2<c1||static_cast<std::int64_t>(r2-r1+1)*(c2-c1+1)>maxMutations)throw std::runtime_error("Clear range is reversed or exceeds 10000 cells");
 }else if(type=="set_metadata"){
  knownFields(op,{"type","key","value"});const auto key=boundedText(op.at("key"),1,64,"key");
  if(std::find(metadataKeys.begin(),metadataKeys.end(),key)==metadataKeys.end())throw std::runtime_error("Unsupported workbook metadata: "+key);
  if(book.contains(key)&&book.at(key).dump().size()>metadataLimit)throw std::runtime_error("Metadata is too large to inspect safely; full replacement is unavailable");
  auto& value=op.at("value");
  const bool array=key=="charts"||key=="pivots"||key=="scenarios"||key=="links";
  if(array?!value.is_array():!value.is_object())throw std::runtime_error("Invalid metadata shape for "+key);
  if(value.dump().size()>metadataLimit)throw std::runtime_error("Metadata exceeds 256 KiB");
  if(key=="charts"){
   const auto existing=book.value("charts",Json::array());std::set<std::string> ids;
   for(auto& chart:value){
    const auto id=boundedText(chart.at("id"),1,128,"chart.id");if(!ids.insert(id).second)throw std::runtime_error("Chart ids must be unique");
    const auto original=std::find(existing.begin(),existing.end(),chart);
    if(original==existing.end())chart=normalizedChart(chart,book);
   }
  }else if(key=="vba"){
   const auto old=book.value("vba",Json::object());
   if(value.contains("enabled")&&(!value.at("enabled").is_boolean()||(value.at("enabled").get<bool>()&&!old.value("enabled",false))))throw std::runtime_error("Only the user can enable macros in the desktop application");
   auto merged=old;merged.update(value);value=std::move(merged);
  }else if(key=="names")for(const auto& [name,reference]:value.items()){if(name.empty()||!reference.is_string())throw std::runtime_error("Names map nonempty identifiers to reference strings");}
 }else throw std::runtime_error("Unsupported workbook operation: "+type);
}
void validateBudget(const Json& operations){
 std::int64_t count=0;
 for(const auto& op:operations){
  count+=op.at("type")=="clear_range"?static_cast<std::int64_t>(op.at("row2").get<int>()-op.at("row1").get<int>()+1)*(op.at("col2").get<int>()-op.at("col1").get<int>()+1):1;
  if(count>maxMutations)throw std::runtime_error("The complete preview exceeds 10000 operations/cells");
 }
}
std::string folded(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
bool editableFinancial(const Json& definition){
 if(definition.value("version",Json())!=1||!definition.value("kind",Json()).is_string())return false;
 const auto& kind=definition.at("kind").get_ref<const std::string&>();return kind=="assumption_set"||kind=="scenario"||kind=="forecast"||kind=="scenario_comparison"||kind=="sensitivity"||kind=="goal_seek"||kind=="solver";
}
Json financialIdentity(const Json& definition){
 Json result=Json::object();for(const auto* key:{"version","id","name","kind"})if(definition.contains(key)){const auto& value=definition.at(key);if(value.is_string()){auto text=value.get<std::string>();if(text.size()>256){std::size_t end=256;while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;text.resize(end);}result[key]=std::move(text);}else if(value.is_number_integer())result[key]=value;}
 return result;
}
Json editableFinancialDefinition(const Json& definition){
 auto result=financialIdentity(definition);if(!editableFinancial(definition))return result;
 const auto kind=definition.at("kind").get<std::string>();
 const auto copy=[&](const char* key){if(definition.contains(key))result[key]=definition.at(key);};
 if(kind=="scenario"||kind=="assumption_set")copy("cells");
 else if(kind=="goal_seek"||kind=="solver")copy("specification");
 else if(kind=="forecast")for(const auto* key:{"periods","startPeriod","drivers","assumptionSetId","output"})copy(key);
 else if(kind=="scenario_comparison")for(const auto* key:{"scenarioIds","outputs","includeBaseline","output"})copy(key);
 else for(const auto* key:{"inputs","result","output"})copy(key);
 if(result.contains("output")){result["output"].erase("row2");result["output"].erase("col2");}
 if(result.dump().size()>metadataLimit)throw std::runtime_error("Financial definition exceeds256 KiB; incomplete definitions cannot be edited");return result;
}
void validateFinancialInput(const Json& definition,bool reports=false){
 if(definition.dump().size()>262144)throw std::runtime_error("Financial definition exceeds256 KiB");
 const auto kind=boundedText(definition.at("kind"),1,64,"kind");boundedInteger(definition.at("version"),1,1,"version");boundedText(definition.at("id"),1,128,"id");boundedText(definition.at("name"),1,256,"name");
 if(kind=="scenario"||kind=="assumption_set"){knownFields(definition,{"version","id","name","kind","cells"});if(!definition.at("cells").is_array())throw std::runtime_error("Typed assumptions require input cells");for(const auto& cell:definition.at("cells"))knownFields(cell,{"sheet","row","col","type","value"});}
 else if(kind=="goal_seek"||kind=="solver"){
  knownFields(definition,{"version","id","name","kind","specification"});const auto& spec=definition.at("specification");
  if(kind=="goal_seek"){knownFields(spec,{"changing","output","target","start","lower","upper","tolerance","relativeTolerance","maxIterations","maxEvaluations"});for(const auto* key:{"changing","output"})knownFields(spec.at(key),{"sheet","row","col"});}
  else{knownFields(spec,{"objective","goal","target","method","variables","constraints","tolerance","maxEvaluations","seed"});knownFields(spec.at("objective"),{"sheet","row","col"});for(const auto& variable:spec.at("variables"))knownFields(variable,{"sheet","row","col","start","lower","upper","integer","binary"});if(spec.contains("constraints"))for(const auto& constraint:spec.at("constraints"))knownFields(constraint,{"sheet","row","col","relation","value"});}
 }else if(reports&&kind=="forecast"){
  knownFields(definition,{"version","id","name","kind","periods","startPeriod","drivers","assumptionSetId","output"});for(const auto& driver:definition.at("drivers")){knownFields(driver,{"name","base","growth","numberFormat"});for(const auto* key:{"base","growth"})if(driver.at(key).is_object())knownFields(driver.at(key),{"sheet","row","col"});}knownFields(definition.at("output"),{"sheet","row1","col1"});
 }else if(reports&&kind=="scenario_comparison"){
  knownFields(definition,{"version","id","name","kind","scenarioIds","outputs","includeBaseline","output"});for(const auto& output:definition.at("outputs"))knownFields(output,{"sheet","row","col","label","numberFormat"});knownFields(definition.at("output"),{"sheet","row1","col1"});
 }else if(reports&&kind=="sensitivity"){
  knownFields(definition,{"version","id","name","kind","inputs","result","output"});for(const auto& input:definition.at("inputs"))knownFields(input,{"sheet","row","col","label","values","numberFormat"});knownFields(definition.at("result"),{"sheet","row","col","label","numberFormat"});knownFields(definition.at("output"),{"sheet","row1","col1"});
 }else throw std::runtime_error("This kind requires running its financial report builder");
}
CalculationContext financialToolContext(const Json& book,const std::function<CalculationContext(const Json&)>& prepare,const std::function<Json(const Json&)>& calculate,const std::function<bool(std::uint64_t)>& current){
 CalculationContext context;
 if(prepare)context=prepare(book);
 else{
  if(!calculate)throw std::runtime_error("Financial calculation is unavailable");const auto baseline=calculate(book);const auto metadata=baseline.value("calculationContext",Json::object());
  if(!metadata.contains("generation")||!metadata.at("generation").is_number_integer()||(!metadata.at("generation").is_number_unsigned()&&metadata.at("generation").get<std::int64_t>()<0))throw std::runtime_error("Financial workflows require a bound host calculation context");
  if(!metadata.value("links",Json::array()).empty()||(book.contains("links")&&!book.at("links").empty()))throw std::runtime_error("The host must prepare immutable linked financial inputs");
  context.snapshot=book;context.generation=metadata.at("generation").get<std::uint64_t>();context.allowCachedLinks=metadata.value("allowCachedLinks",false);
 }
 if(!current||!current(context.generation)||context.snapshot.value("revision",Json())!=book.at("revision"))throw std::runtime_error("Financial calculation context changed; prepare the proposal again");context.generationIsCurrent=current;
 const auto evaluate=calculate?calculate:context.evaluator;if(!evaluate)throw std::runtime_error("Financial calculation is unavailable");const auto generation=context.generation;const bool allowed=context.allowCachedLinks;
 context.evaluator=[evaluate,current,generation,allowed](const Json& candidate){
  if(!current(generation))throw std::runtime_error("Financial calculation context changed");auto result=evaluate(candidate);const auto metadata=result.value("calculationContext",Json::object());
  if(!current(generation)||!metadata.contains("generation")||!metadata.at("generation").is_number_integer()||metadata.at("generation")!=generation||metadata.value("allowCachedLinks",false)!=allowed)throw std::runtime_error("Financial calculation context or permission changed");
  if(!result.contains("diagnostics"))result["diagnostics"]=Json::array();for(const auto& item:metadata.value("diagnostics",Json::array()))if(std::find(result["diagnostics"].begin(),result["diagnostics"].end(),item)==result["diagnostics"].end())result["diagnostics"].push_back(item);
  bool saved=false;for(const auto& diagnostic:result.at("diagnostics"))if(diagnostic.is_object()&&diagnostic.value("code",std::string()).starts_with("external_link_"))saved=true;for(const auto& link:metadata.value("links",Json::array())){const auto state=link.value("state",std::string());if(state=="cached"||state=="unavailable"||state=="missing")saved=true;}
  if(saved&&!allowed)throw std::runtime_error("cached_link_consent_required: financial results require explicit current desktop permission before using saved linked values");return result;
 };
 return context;
}
PivotGeneration calculatedPivot(const Json& book,const Json& calculated,const Json& definition,const std::function<bool(std::uint64_t)>& current){
 const auto context=calculated.value("calculationContext",Json::object());const bool bound=context.is_object()&&context.contains("generation");std::uint64_t generation=0;if(bound){const auto& value=context.at("generation");if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<std::int64_t>()<0))throw std::runtime_error("Invalid linked calculation context generation");generation=value.get<std::uint64_t>();}
 if(bound&&current&&!current(generation))throw std::runtime_error("Linked calculation context changed; prepare the report again");
 Json diagnostics=calculated.value("diagnostics",Json::array());for(const auto& diagnostic:context.value("diagnostics",Json::array()))if(std::find(diagnostics.begin(),diagnostics.end(),diagnostic)==diagnostics.end())diagnostics.push_back(diagnostic);
 bool cached=false;for(const auto& diagnostic:diagnostics)if(diagnostic.is_object()&&diagnostic.value("code",std::string()).starts_with("external_link_"))cached=true;
 const bool allowed=context.value("allowCachedLinks",false)&&bound&&current&&current(generation);
 if(cached&&!allowed)throw std::runtime_error("cached_link_consent_required: saved or unavailable linked inputs require explicit desktop permission before creating a financial report");
 auto generated=generatePivot(book,calculated,definition,generation,[&]{return bound&&current&&!current(generation);});
 Json links=Json::array();for(const auto& link:context.value("links",Json::array())){Json summary=Json::object();for(const auto* key:{"name","path","state"})if(link.contains(key))summary[key]=link.at(key);links.push_back(std::move(summary));}
 generated.definition["provenance"]={{"links",std::move(links)},{"diagnostics",std::move(diagnostics)},{"allowCachedLinks",allowed}};
 if(bound&&current&&!current(generation))throw std::runtime_error("Linked calculation context changed; the report candidate was discarded");return generated;
}
}
WorkbookTools::WorkbookTools(WorkbookSession& session,Json functions):session_(session),availableFunctions_(std::move(functions)),base_(session.snapshotView().at("revision")){}
void WorkbookTools::reset(){operations_=std::make_shared<const Json>(Json::array());pending_=false;base_=session_.snapshotView().at("revision").get<std::uint64_t>();}
WorkbookTools::ProposalState WorkbookTools::proposalState()const noexcept{return {base_,operations_};}
bool WorkbookTools::adoptProposal(ProposalState state,std::string* error){
 if(!state.operations||!state.operations->is_array()||state.operations->size()>maxMutations||state.baseRevision!=session_.snapshotView().at("revision").get<std::uint64_t>()||!session_.previewView()){if(error)*error="Proposal state requires a matching revision and an adopted Session preview";return false;}
 base_=state.baseRevision;operations_=std::move(state.operations);pending_=true;if(error)error->clear();return true;
}
Json WorkbookTools::definitions(){
 Json tools=Json::array();
 const auto add=[&](const char* name,const char* description,Json schema){tools.push_back({{"type","function"},{"name",name},{"description",description},{"inputSchema",std::move(schema)}});};
 add("workbook_v1_capabilities","Discover built-in native workbook editing tools, exact operation schemas, limits and financial-model/chart examples. Call before preparing workbook edits; no external skill, file access or installation is needed.",object(Json::object()));
 add("workbook_v1_read","Read bounded native cells and workbook metadata, including charts. During a proposal this reads the pending preview; revision remains the live base for later preview calls. Coordinates and rectangles are 1-based and inclusive. At most 10000 cells/1 MiB cell JSON; each metadata value is limited to 256 KiB. metadataTruncated names omitted fields, which cannot be replaced.",object({{"sheet",identifier()},{"row1",integer(1,maxRows)},{"col1",integer(1,maxColumns)},{"row2",integer(1,maxRows)},{"col2",integer(1,maxColumns)},
  {"metadata",{{"type","array"},{"maxItems",7},{"uniqueItems",true},{"items",{{"type","string"},{"enum",metadataKeys}}}}}}));
 add("workbook_v1_search","Search live or pending preview cell inputs for text. Returns stable worksheet ids, cell coordinates, the live base revision and up to 1000 matches.",object({{"query",{{"type","string"},{"minLength",1},{"maxLength",4096}}}},{"query"}));
 add("workbook_v1_report","Read a native report slice within 1 MiB, including its complete editable definition (at most 256 KiB). Oversized definitions refuse safely; use bounded member reads to inspect large selections. Reads the pending proposal and never returns source records.",object({{"id",identifier()},{"rowOffset",integer(0,250000)},{"rowLimit",integer(1,1000)},{"colOffset",integer(0,maxColumns)},{"colLimit",integer(1,32)}},{"id"}));
 add("workbook_v1_reports","List up to 200 native report summaries, including preserved read-only imports. Returns stable ids, locations and field names without source records or member caches.",object({{"offset",integer(0,100000)},{"limit",integer(1,200)}}));
 add("workbook_v1_pivot_members","Search a report's retained source-field members. Returns up to 200 typed items and at most 1 MiB, with selection and missing-item state. Query searches the whole field cache; offset paginates matching members. Reads the pending proposal when present.",object({{"id",identifier()},{"field",textSchema(256)},{"query",textSchema(4096)},{"offset",integer(0,100000)},{"limit",integer(1,200)}},{"id","field"}));
 add("workbook_v1_financial_models","List up to200 financial model summaries from the live or proposed workbook, optionally filtering by kind. Generated results and source caches are omitted; unsupported versions remain read-only.",object({{"offset",integer(0,100000)},{"limit",integer(1,200)},{"kind",textSchema(64)}}));
 add("workbook_v1_financial_model","Read one complete editable financial setup, at most256 KiB. Generated results, provenance and caches are omitted. Unsupported definitions return bounded read-only identities.",object({{"id",identifier()}},{"id"}));
 add("workbook_v1_preview","Create or extend one isolated, revision-checked native workbook proposal. When asked to create/edit a model or chart, use this tool to do it. set_range writes financial data/formulas; add_chart appends an editable chart. Consecutive calls at the same live revision accumulate in one proposal. Only the user can accept/discard it; saving remains a desktop action.",
  object({{"revision",{{"type","integer"},{"minimum",0}}},{"operations",{{"type","array"},{"minItems",1},{"maxItems",maxMutations},{"items",{{"oneOf",operationSchemas()}}}}}},{"revision","operations"}));
 return tools;
}
Json WorkbookTools::invoke(std::string tool,const Json& args){
 try{
  if(!args.is_object())throw std::runtime_error("Workbook tool arguments must be an object");
  const auto& live=session_.snapshotView();
  if(!session_.hasPreview())reset();
  if(pending_&&base_!=live.at("revision").get<std::uint64_t>()){reset();throw std::runtime_error("Workbook changed; read the current revision and prepare a new preview");}
  const auto& active=pending_?*session_.previewView():live;
  if(tool=="workbook_v1_capabilities"){
   knownFields(args,{});
   Json sheets=Json::array();for(const auto& sheet:active.at("sheets"))sheets.push_back({{"id",sheet.at("id")},{"name",sheet.at("name")}});
   Json result={{"version","workbook.v1"},{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"sheets",sheets},
    {"operations",{"set_cell","set_range","set_style","add_sheet","rename_sheet","clear_range","add_chart","set_metadata"}},
    {"operationSchemas",operationSchemas()},{"formulas",availableFunctions_},
    {"bounds",{{"previewOperations",maxMutations},{"previewCells",maxMutations},{"readCells",maxMutations},{"readCellBytes",1048576},{"metadataValueBytes",metadataLimit},{"searchResults",1000},{"searchCellBytes",1048576}}},
    {"metadataFields",metadataKeys},
    {"workflow",{{"writesCommitted",false},{"manualSkillActivationRequired",false},{"coordinates","1-based, inclusive"},
     {"steps",{"Read the native workbook and stable worksheet ids.","Create data and formulas with set_range, then add_chart using that table.","Preview calls at the same live revision accumulate; reads inspect the combined preview.","After acceptance/discard or revision conflict read again. Only the user can accept or save."}}}},
    {"examples",{{"financialModel",financialExample(live.at("revision"),active.at("sheets")[0].at("id"))}}}};
   for(const auto* name:{"create_pivot","update_pivot","refresh_pivot","move_pivot","delete_pivot","create_pivot_chart","update_pivot_chart","delete_pivot_chart","save_financial_model","run_financial_model","apply_financial_scenario","delete_financial_model"})result["operations"].push_back(name);result["bounds"]["pivotSourceRecords"]=100000;result["bounds"]["pivotOutputCells"]=250000;result["bounds"]["pivotMemberResults"]=200;result["bounds"]["pivotMemberBytes"]=1048576;result["bounds"]["reportSummaries"]=200;result["bounds"]["financialModels"]=200;result["bounds"]["financialDefinitionBytes"]=metadataLimit;
   if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_search"){
   knownFields(args,{"query"});const auto query=folded(boundedText(args.at("query"),1,4096,"query"));Json matches=Json::array();std::size_t bytes=0;bool truncated=false;
   for(const auto& sheet:active.at("sheets"))for(const auto& cell:sheet.at("cells"))if(folded(cell.value("input",std::string())).find(query)!=std::string::npos){
    const auto size=cell.dump().size();if(matches.size()>=1000||bytes+size>1048576){truncated=true;continue;}bytes+=size;matches.push_back({{"sheet",sheet.at("id")},{"cell",cell}});
   }
   Json result={{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"matches",matches},{"truncated",truncated}};
   if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_read"){
   knownFields(args,{"sheet","row1","col1","row2","col2","metadata"});
   if(args.contains("sheet"))requireSheet(active,args.at("sheet"));
   const auto r1=boundedInteger(args.value("row1",Json(1)),1,maxRows,"row1"),r2=boundedInteger(args.value("row2",Json(maxRows)),1,maxRows,"row2"),
    c1=boundedInteger(args.value("col1",Json(1)),1,maxColumns,"col1"),c2=boundedInteger(args.value("col2",Json(maxColumns)),1,maxColumns,"col2");
   if(r2<r1||c2<c1)throw std::runtime_error("Read range is reversed");
   Json sheets=Json::array();int count=0;std::size_t bytes=0;bool truncated=false;
   for(const auto& sheet:active.at("sheets")){
    if(args.contains("sheet")&&args.at("sheet")!=sheet.at("id"))continue;
    Json cells=Json::array();
    for(const auto& cell:sheet.at("cells"))if(cell.at("row")>=r1&&cell.at("row")<=r2&&cell.at("col")>=c1&&cell.at("col")<=c2){
     const auto size=cell.dump().size();if(count>=maxMutations||bytes+size>1048576){truncated=true;continue;}++count;bytes+=size;cells.push_back(cell);
    }
    sheets.push_back({{"id",sheet.at("id")},{"name",sheet.at("name")},{"cells",cells}});
   }
   Json metadata=Json::object(),omitted=Json::array();Json requested=args.value("metadata",Json(metadataKeys));
   if(!requested.is_array()||requested.size()>metadataKeys.size())throw std::runtime_error("metadata must list up to seven supported field names");
   std::set<std::string> unique;
   for(const auto& field:requested){const auto key=boundedText(field,1,64,"metadata field");if(std::find(metadataKeys.begin(),metadataKeys.end(),key)==metadataKeys.end()||!unique.insert(key).second)throw std::runtime_error("Unsupported or repeated metadata field");if(active.contains(key)){if(active.at(key).dump().size()>metadataLimit)omitted.push_back(key);else metadata[key]=active.at(key);}}
   auto charts=active.value("charts",Json::array());const bool chartsTruncated=charts.dump().size()>metadataLimit;if(chartsTruncated)charts=Json::array();
   Json result={{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"sheets",sheets},{"charts",charts},{"chartsTruncated",chartsTruncated},
    {"metadata",metadata},{"metadataTruncated",omitted},{"truncated",truncated}};
   if(calculate){
    const auto calculated=calculate(active);
    if(calculated.value("revision",Json())!=active.at("revision"))result["calculation"]={{"status","stale"},{"diagnostics",{"Calculation results did not match the displayed workbook revision"}}};
    else if(calculated.value("status",std::string())!="ok")result["calculation"]={{"status",calculated.value("status",std::string("error"))},{"diagnostics",calculated.value("diagnostics",Json::array())}};
    else{
     std::map<std::tuple<std::string,int,int>,Json> values;
     for(const auto& value:calculated.value("values",Json::array()))values[{value.at("sheet").get<std::string>(),value.at("row").get<int>(),value.at("col").get<int>()}]=value.at("value");
     for(auto& sheet:result["sheets"])for(auto& cell:sheet["cells"]){const auto found=values.find({sheet.at("id").get<std::string>(),cell.at("row").get<int>(),cell.at("col").get<int>()});if(found!=values.end())cell["value"]=found->second;}
     result["calculation"]={{"status","ok"}};
    }
   }
   if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_financial_model"||tool=="workbook_v1_financial_models"){
   const bool single=tool=="workbook_v1_financial_model";if(single)knownFields(args,{"id"});else knownFields(args,{"offset","limit","kind"});
   const auto id=single?boundedText(args.at("id"),1,128,"id"):std::string();const auto kind=args.contains("kind")?boundedText(args.at("kind"),1,64,"kind"):std::string();
   const int offset=single?0:boundedInteger(args.value("offset",Json(0)),0,100000,"offset"),limit=single?1:boundedInteger(args.value("limit",Json(100)),1,200,"limit");Json models=Json::array();std::size_t total=0,bytes=0;
   if(active.contains("financialModels"))for(const auto& model:active.at("financialModels")){
    if(single){if(model.value("id",Json())!=id)continue;Json result={{"revision",live.at("revision")},{"view",pending_?"preview":"live"},{"definition",editableFinancialDefinition(model)},{"readOnly",!editableFinancial(model)},{"stale",model.value("stale",false)}};if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;}
    if(!kind.empty()&&model.value("kind",Json())!=kind)continue;const auto index=total++;if(index<static_cast<std::size_t>(offset)||models.size()>=static_cast<std::size_t>(limit))continue;
    auto summary=financialIdentity(model);summary["readOnly"]=!editableFinancial(model);summary["stale"]=model.value("stale",false);const auto size=summary.dump().size();if(bytes+size>1047552)continue;bytes+=size;models.push_back(std::move(summary));
   }
   if(single)throw std::runtime_error("Financial model id was not found");Json result={{"revision",live.at("revision")},{"view",pending_?"preview":"live"},{"models",std::move(models)},{"total",total},{"offset",offset}};result["truncated"]=static_cast<std::size_t>(offset)+result["models"].size()<total;if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_reports"){
   knownFields(args,{"offset","limit"});const int offset=boundedInteger(args.value("offset",Json(0)),0,100000,"offset"),limit=boundedInteger(args.value("limit",Json(100)),1,200,"limit");Json reports=Json::array();std::size_t total=0,bytes=0;
   const auto append=[&](Json summary){const auto index=total++;if(index<static_cast<std::size_t>(offset)||reports.size()>=static_cast<std::size_t>(limit))return;const auto size=summary.dump().size();if(bytes+size>1048576)return;bytes+=size;reports.push_back(std::move(summary));};
   const auto rectangle=[](const Json& range){Json out;for(const auto* key:{"sheet","row1","col1","row2","col2"})if(range.contains(key))out[key]=range.at(key);return out;};
   if(active.contains("pivots"))for(const auto& pivot:active.at("pivots"))if(pivot.value("version",0)==1){Json fields=Json::array();const auto& cacheFields=pivot.at("cache").at("fields");for(std::size_t i=0;i<std::min<std::size_t>(cacheFields.size(),10);++i)fields.push_back(cacheFields[i].at("name"));append({{"id",pivot.at("id")},{"name",pivot.at("name")},{"readOnly",false},{"stale",pivot.value("stale",false)},{"source",rectangle(pivot.at("source"))},{"output",rectangle(pivot.at("output"))},{"fields",std::move(fields)},{"fieldsTruncated",cacheFields.size()>10},{"recordCount",pivot.at("cache").at("records").size()}});}
   if(active.contains("opaquePivots"))for(const auto& pivot:active.at("opaquePivots")){auto reason=pivot.value("reason",std::string());if(reason.size()>4096)reason.resize(4096);append({{"id","opaque:"+pivot.value("part",std::string())},{"name",pivot.value("name",std::string())},{"sheet",pivot.value("sheet",std::string())},{"readOnly",true},{"reason",std::move(reason)}});}
   Json result={{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"reports",std::move(reports)},{"total",total},{"offset",offset}};result["truncated"]=static_cast<std::size_t>(offset)+result["reports"].size()<total;if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_pivot_members"){
   knownFields(args,{"id","field","query","offset","limit"});const auto id=boundedText(args.at("id"),1,128,"id"),name=boundedText(args.at("field"),1,256,"field"),query=folded(boundedText(args.value("query",Json("")),0,4096,"query"));const int offset=boundedInteger(args.value("offset",Json(0)),0,100000,"offset"),limit=boundedInteger(args.value("limit",Json(200)),1,200,"limit");
   const Json* pivot=nullptr;if(active.contains("pivots"))for(const auto& candidate:active.at("pivots"))if(candidate.value("version",0)==1&&candidate.at("id")==id){pivot=&candidate;break;}if(!pivot)throw std::runtime_error("Editable native pivot id was not found");const Json* field=nullptr;for(const auto& candidate:pivot->at("cache").at("fields"))if(candidate.at("name")==name){field=&candidate;break;}if(!field)throw std::runtime_error("Pivot source field was not found");
   const auto less=[](const Json* a,const Json* b){const auto& aType=a->at("type").get_ref<const std::string&>();const auto& bType=b->at("type").get_ref<const std::string&>();return aType!=bType?aType<bType:a->at("value")<b->at("value");};std::set<const Json*,decltype(less)> available(less),selected(less),missing(less);for(const auto& value:field->at("items"))available.insert(&value);bool all=true;
   if(pivot->contains("filters"))for(const auto& filter:pivot->at("filters"))if(filter.at("field")==name){all=filter.value("all",true);if(filter.contains("items"))for(const auto& value:filter.at("items"))selected.insert(&value);if(filter.contains("missingItems"))for(const auto& value:filter.at("missingItems"))if(!available.contains(&value))missing.insert(&value);break;}
   Json items=Json::array();std::size_t matchCount=0,bytes=0;
   const auto append=[&](const Json& value,bool absent){std::string label;if(value.at("type")=="blank")label="(blank)";else if(value.at("value").is_string())label=value.at("value").get<std::string>();else if(value.at("value").is_boolean())label=value.at("value").get<bool>()?"TRUE":"FALSE";else label=value.at("value").dump();if(folded(std::move(label)).find(query)==std::string::npos)return;const auto index=matchCount++;if(index<static_cast<std::size_t>(offset)||items.size()>=static_cast<std::size_t>(limit))return;Json item={{"item",value},{"missing",absent},{"selected",selected.contains(&value)||(!absent&&all)}};const auto size=item.dump().size();if(bytes+size>1048576)return;bytes+=size;items.push_back(std::move(item));};
   for(const auto& value:field->at("items"))append(value,false);for(const auto* value:missing)append(*value,true);
   Json result={{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"id",id},{"field",name},{"numberFormat",field->value("numberFormat",std::string("General"))},{"items",std::move(items)},{"total",available.size()+missing.size()},{"matchCount",matchCount},{"offset",offset},{"all",all}};result["truncated"]=static_cast<std::size_t>(offset)+result["items"].size()<matchCount;if(pending_)result["previewRevision"]=session_.previewView()->at("revision");return result;
  }
  if(tool=="workbook_v1_report"){
   knownFields(args,{"id","rowOffset","rowLimit","colOffset","colLimit"});const auto id=boundedText(args.at("id"),1,128,"id");const Json* selected=nullptr;
   if(active.contains("pivots"))for(const auto& pivot:active.at("pivots"))if(pivot.value("version",0)==1&&pivot.at("id")==id){selected=&pivot;break;}if(!selected)throw std::runtime_error("Native pivot id was not found");
   Json definition=Json::object();for(const auto& [key,value]:selected->items())if(key!="cache"&&key!="layout")definition[key]=value;
   const auto definitionBytes=definition.dump().size();if(definitionBytes>metadataLimit)throw std::runtime_error("Report definition exceeds 256 KiB; use bounded member reads rather than replacing an incomplete filter selection");
   const int offset=boundedInteger(args.value("rowOffset",Json(0)),0,250000,"rowOffset"),limit=boundedInteger(args.value("rowLimit",Json(100)),1,1000,"rowLimit"),firstCol=boundedInteger(args.value("colOffset",Json(0)),0,maxColumns,"colOffset"),columns=boundedInteger(args.value("colLimit",Json(32)),1,32,"colLimit");
   Json rows=Json::array(),keys=Json::array();std::size_t bytes=definitionBytes+512;const auto& data=selected->at("layout").at("data");for(std::size_t r=offset;r<data.size()&&r<static_cast<std::size_t>(offset+limit);++r){Json values=Json::array();for(std::size_t c=firstCol;c<data[r].size()&&c<static_cast<std::size_t>(firstCol+columns);++c)values.push_back(data[r][c]);const auto& key=selected->at("layout").at("rowKeys")[r];const auto rowBytes=values.dump().size()+key.dump().size()+4;if(bytes+rowBytes>1048576)break;bytes+=rowBytes;rows.push_back(std::move(values));keys.push_back(key);}
   const auto next=static_cast<std::size_t>(offset)+rows.size();return {{"revision",live.at("revision")},{"view",!pending_?"live":"preview"},{"definition",std::move(definition)},{"rowKeys",std::move(keys)},{"data",std::move(rows)},{"rowCount",data.size()},{"nextRowOffset",next},{"truncated",next<data.size()},{"stale",selected->value("stale",false)}};
  }
  if(tool!="workbook_v1_preview")throw std::runtime_error("Unknown workbook tool");
  knownFields(args,{"revision","operations"});
  const auto expected=revision(args.at("revision"));if(expected!=live.at("revision"))throw std::runtime_error("Workbook revision conflict");
  if(!args.at("operations").is_array()||args.at("operations").empty()||args.at("operations").size()>maxMutations)throw std::runtime_error("Preview requires 1 to 10000 operations");
  Json expanded=*operations_,financialResults=Json::array();WorkbookSession scratch(active);
  for(const auto& op:args.at("operations")){
   Json batch=Json::array();
   if(op.at("type")=="set_range"){
    knownFields(op,{"type","sheet","row","col","values"});requireSheet(scratch.snapshot(),op.at("sheet"));
    int row=boundedInteger(op.at("row"),1,maxRows,"row"),firstCol=boundedInteger(op.at("col"),1,maxColumns,"col");
    const auto& rows=op.at("values");
    if(!rows.is_array()||rows.empty()||rows.size()>maxMutations||!rows[0].is_array()||rows[0].empty())throw std::runtime_error("values must be a nonempty rectangular matrix");
    const auto width=rows[0].size();
    if(width>maxMutations||rows.size()*width>maxMutations||rows.size()>static_cast<std::size_t>(maxRows-row+1)||width>static_cast<std::size_t>(maxColumns-firstCol+1))throw std::runtime_error("Range exceeds worksheet bounds or 10000 cells");
    for(const auto& values:rows){
     if(!values.is_array()||values.size()!=width)throw std::runtime_error("values must be a nonempty rectangular matrix");
     int col=firstCol;
     for(const auto& value:values)batch.push_back({{"type","set_cell"},{"sheet",op.at("sheet")},{"row",row},{"col",col++},{"input",input(value)}});
     ++row;
    }
   }else if(op.at("type")=="save_financial_model"||op.at("type")=="run_financial_model"||op.at("type")=="apply_financial_scenario"||op.at("type")=="delete_financial_model"){
    const auto type=op.at("type").get<std::string>();const bool save=type=="save_financial_model",run=type=="run_financial_model";if(save)knownFields(op,{"type","definition"});else if(run)knownFields(op,{"type","id","definition"});else knownFields(op,{"type","id"});
    if(run&&op.contains("id")==op.contains("definition"))throw std::runtime_error("Run requires exactly one saved model id or definition");
    const auto& snapshot=scratch.snapshotView();Json definition;if(op.contains("definition")){validateFinancialInput(op.at("definition"),run);definition=op.at("definition");}else{const auto id=boundedText(op.at("id"),1,128,"id");const Json* selected=nullptr;if(snapshot.contains("financialModels"))for(const auto& candidate:snapshot.at("financialModels"))if(candidate.value("id",Json())==id){selected=&candidate;break;}if(!selected||!editableFinancial(*selected))throw std::runtime_error("Editable saved financial model id was not found");definition=editableFinancialDefinition(*selected);}
    auto context=financialToolContext(snapshot,financialContext,calculate,calculationContextIsCurrent);const auto result=executeFinancialWorkflow(snapshot,definition,save?"save":run?"run":type=="apply_financial_scenario"?"apply":"delete",context,[&]{return !context.current();});
    if(!context.current()||result.value("revision",Json())!=snapshot.at("revision")||result.value("contextGeneration",Json())!=context.generation)throw std::runtime_error("Financial workflow returned a foreign source generation");
    const auto& analysis=result.at("analysis");if(!analysis.value("success",false))throw std::runtime_error("Financial workflow did not complete: "+analysis.dump());if(analysis.dump().size()>262144)throw std::runtime_error("Financial analysis exceeds256 KiB");financialResults.push_back({{"id",definition.at("id")},{"kind",definition.at("kind")},{"analysis",analysis}});if(financialResults.dump().size()>1000000)throw std::runtime_error("Financial analysis reply exceeds1 MiB");batch=result.at("operations");
   }else if(op.at("type")=="create_pivot"){
    knownFields(op,{"type","definition"});const auto snapshot=scratch.snapshot();const auto& definition=op.at("definition");knownFields(definition,{"id","name","source","rows","columns","filters","values","rowGrandTotals","columnGrandTotals","output"});
    if(snapshot.contains("pivots"))for(const auto& pivot:snapshot.at("pivots"))if(pivot.value("id",Json())==definition.at("id"))throw std::runtime_error("Pivot id already exists");if(!calculate)throw std::runtime_error("Report calculation is unavailable");batch=pivotOperations(snapshot,calculatedPivot(snapshot,calculate(snapshot),definition,calculationContextIsCurrent));
   }else if(op.at("type")=="update_pivot"||op.at("type")=="refresh_pivot"){
    const bool refresh=op.at("type")=="refresh_pivot";if(refresh)knownFields(op,{"type","id"});else knownFields(op,{"type","id","changes"});const auto snapshot=scratch.snapshot();const auto id=boundedText(op.at("id"),1,128,"id");Json definition;bool found=false;if(snapshot.contains("pivots"))for(const auto& pivot:snapshot.at("pivots"))if(pivot.value("version",0)==1&&pivot.value("id",std::string())==id){definition=Json::object();for(const auto& [key,value]:pivot.items())if(key!="cache"&&key!="layout")definition[key]=value;found=true;break;}if(!found)throw std::runtime_error("Editable pivot id was not found");if(!refresh){knownFields(op.at("changes"),{"name","source","rows","columns","filters","values","rowGrandTotals","columnGrandTotals","output"});definition.update(op.at("changes"));}if(!calculate)throw std::runtime_error("Report calculation is unavailable");batch=pivotOperations(snapshot,calculatedPivot(snapshot,calculate(snapshot),definition,calculationContextIsCurrent));
   }else if(op.at("type")=="move_pivot"){
    knownFields(op,{"type","id","output"});const auto& snapshot=scratch.snapshotView();const auto id=boundedText(op.at("id"),1,128,"id");const Json* previous=nullptr;if(snapshot.contains("pivots"))for(const auto& pivot:snapshot.at("pivots"))if(pivot.value("version",0)==1&&pivot.value("id",std::string())==id){previous=&pivot;break;}if(!previous)throw std::runtime_error("Editable pivot id was not found");
    const auto& output=op.at("output");knownFields(output,{"sheet","row1","col1"});requireSheet(snapshot,output.at("sheet"));boundedInteger(output.at("row1"),1,maxRows,"output.row1");boundedInteger(output.at("col1"),1,maxColumns,"output.col1");auto definition=*previous;definition["output"]=output;
    auto generated=generatePivotFromCache(snapshot,definition,previous->at("cache").at("fields"),previous->at("cache").at("records"),previous->at("cache").value("contextGeneration",std::uint64_t{}));generated.definition["stale"]=previous->value("stale",false);batch=pivotOperations(snapshot,generated);
   }else if(op.at("type")=="delete_pivot"){
    knownFields(op,{"type","id"});batch=deletePivotOperations(scratch.snapshotView(),boundedText(op.at("id"),1,128,"id"));
   }else if(op.at("type")=="create_pivot_chart"){
    knownFields(op,{"type","pivotId","id","chartType","title"});const auto snapshot=scratch.snapshot();auto charts=snapshot.value("charts",Json::array());auto chart=pivotChart(snapshot,boundedText(op.at("pivotId"),1,128,"pivotId"),boundedText(op.at("id"),1,128,"id"),boundedText(op.at("chartType"),1,32,"chartType"));if(op.contains("title"))chart["title"]=boundedText(op.at("title"),0,4096,"title");charts.push_back(std::move(chart));batch.push_back({{"type","set_metadata"},{"key","charts"},{"value",std::move(charts)}});
   }else if(op.at("type")=="update_pivot_chart"||op.at("type")=="delete_pivot_chart"){
    const bool remove=op.at("type")=="delete_pivot_chart";if(remove)knownFields(op,{"type","id"});else knownFields(op,{"type","id","changes"});const auto& snapshot=scratch.snapshotView();const auto id=boundedText(op.at("id"),1,128,"id");auto charts=snapshot.value("charts",Json::array());auto found=std::find_if(charts.begin(),charts.end(),[&](const auto& chart){return chart.value("id",std::string())==id&&chart.contains("pivotId");});if(found==charts.end())throw std::runtime_error("Editable PivotChart id was not found");
    if(remove)charts.erase(found);else{const auto& changes=op.at("changes");knownFields(changes,{"chartType","title","anchor"});auto chart=*found;if(changes.contains("chartType")){const auto type=boundedText(changes.at("chartType"),1,32,"chartType");if(type!="line"&&type!="column"&&type!="bar")throw std::runtime_error("PivotCharts support line, column and bar families");chart["type"]=type;}if(changes.contains("title"))chart["title"]=boundedText(changes.at("title"),0,4096,"title");if(changes.contains("anchor")){knownFields(changes.at("anchor"),{"row","col","width","height","offsetX","offsetY"});chart["anchor"].update(changes.at("anchor"));}const auto pivotId=chart.at("pivotId");chart.erase("pivotId");chart=normalizedChart(std::move(chart),snapshot);chart["pivotId"]=pivotId;pivotChartData(snapshot,chart);*found=std::move(chart);}
    if(charts!=snapshot.value("charts",Json::array()))batch.push_back({{"type","set_metadata"},{"key","charts"},{"value",std::move(charts)}});
   }else if(op.at("type")=="add_chart"){
    knownFields(op,{"type","chart"});
    const auto snapshot=scratch.snapshot();auto charts=snapshot.value("charts",Json::array());auto chart=normalizedChart(op.at("chart"),snapshot);
    for(const auto& existing:charts)if(existing.value("id",Json())==chart.at("id"))throw std::runtime_error("Chart id already exists; choose a new stable id");
    charts.push_back(std::move(chart));
    batch.push_back({{"type","set_metadata"},{"key","charts"},{"value",charts}});
   }else{auto native=op;validateNativeOperation(native,scratch.snapshot());batch.push_back(std::move(native));}
   if(batch.empty())continue;
   if(expanded.size()+batch.size()>maxMutations)throw std::runtime_error("The complete preview exceeds 10000 operations/cells");
   std::string error;if(!scratch.apply(scratch.snapshot().at("revision"),batch,&error))throw std::runtime_error(error);
   for(auto operation:batch){if(operation.at("type")=="replace_pivot")operation["definition"]["cache"]["sourceRevision"]=live.at("revision");if(operation.at("type")=="replace_financial_model")operation["definition"]["sourceRevision"]=live.at("revision");expanded.push_back(std::move(operation));}
   }
  if(expanded.empty()){Json result={{"status","unchanged"},{"revision",live.at("revision")},{"view","live"}};if(!financialResults.empty())result["financialResults"]=std::move(financialResults);return result;}
  validateBudget(expanded);
  std::string error;auto candidate=session_.preview(args.at("revision").get<std::uint64_t>(),expanded,&error);
  if(candidate.is_null())throw std::runtime_error(error);
  operations_=std::make_shared<const Json>(std::move(expanded));base_=expected;pending_=true;if(onPreview)onPreview(*session_.previewView());
  Json reply={{"status","preview"},{"revision",args.at("revision")},{"previewRevision",session_.previewView()->at("revision")},{"view","preview"}};if(!financialResults.empty())reply["financialResults"]=std::move(financialResults);return reply;
 }catch(const std::exception& error){return {{"error",error.what()}};}
}
}
