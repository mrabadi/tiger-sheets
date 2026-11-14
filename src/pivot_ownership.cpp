#include "tigersheets/pivot_ownership.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/references.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
namespace tigersheets {
namespace {
bool owned(const Json& pivot){return pivot.is_object()&&pivot.value("version",0)==1;}
bool inside(const Json& range,int row,int col){return row>=range.at("row1")&&row<=range.at("row2")&&col>=range.at("col1")&&col<=range.at("col2");}
bool overlaps(const Json& a,const Json& b){return a.at("sheet")==b.at("sheet")&&a.at("row1")<=b.at("row2")&&b.at("row1")<=a.at("row2")&&a.at("col1")<=b.at("col2")&&b.at("col1")<=a.at("col2");}
const Json& pivots(const Json& book){static const Json empty=Json::array();return book.contains("pivots")?book.at("pivots"):empty;}
void bounds(const Json& range){const int r1=range.at("row1"),r2=range.at("row2"),c1=range.at("col1"),c2=range.at("col2");if(r1<1||r2<r1||r2>1048576||c1<1||c2<c1||c2>16384)throw std::runtime_error("Invalid pivot rectangle");}
void validateReplacement(const Json& book,const Json& definition,const Json& cells){
 if(!owned(definition)||definition.at("id").get<std::string>().empty()||definition.at("name").get<std::string>().empty())throw std::runtime_error("Pivot requires a stable id, name and supported version");
 const auto id=definition.at("id").get<std::string>();const auto& output=definition.at("output");const auto& source=definition.at("source");bounds(output);bounds(source);
 if(overlaps(output,source))throw std::runtime_error("Pivot output cannot overlap its source");
 for(const auto& other:pivots(book))if(owned(other)&&other.at("id")!=id){if(overlaps(output,other.at("source")))throw std::runtime_error("Pivot output cannot overlap another report's source");if(overlaps(source,other.at("output")))throw std::runtime_error("Pivot source cannot contain another report's generated output");}
 if(!cells.is_array()||cells.size()>250000)throw std::runtime_error("Pivot output exceeds 250,000 cells");
 if(definition.at("cache").at("sourceRevision")!=book.at("revision"))throw std::runtime_error("Pivot refresh revision conflict");
 bool sourceExists=false,destinationExists=false;
 for(const auto& sheet:book.at("sheets")){sourceExists|=sheet.at("id")==source.at("sheet");if(sheet.at("id")!=output.at("sheet"))continue;destinationExists=true;for(const auto& cell:sheet.at("cells"))if(inside(output,cell.at("row"),cell.at("col"))&&cell.value("pivotOwner",std::string())!=id&&(!cell.value("input",std::string()).empty()||(cell.contains("style")&&!cell.at("style").empty())))throw std::runtime_error("Pivot output contains occupied unowned cells; choose another destination");}
 if(!sourceExists||!destinationExists)throw std::runtime_error("Pivot source or destination worksheet was not found");
 for(const auto& other:pivots(book))if(owned(other)&&other.at("id")!=id){if(other.at("name")==definition.at("name"))throw std::runtime_error("Pivot names must be unique");if(overlaps(output,other.at("output")))throw std::runtime_error("Pivot output overlaps another report");}
 std::set<std::pair<int,int>> addresses;
 for(const auto& cell:cells){const int row=cell.at("row"),col=cell.at("col");if(!inside(output,row,col)||!addresses.emplace(row,col).second||!cell.at("input").is_string()||cell.value("pivotOwner",std::string())!=id)throw std::runtime_error("Invalid owned pivot output cell");}
}
}
Json pivotOperations(const Json& book,const PivotGeneration& generation){
 validateReplacement(book,generation.definition,generation.cells);
 const auto sameExcept=[](const Json& a,const Json& b,const std::set<std::string>& ignored){if(!a.is_object()||!b.is_object())return false;for(const auto* side:{&a,&b})for(const auto& [key,value]:side->items())if(!ignored.contains(key)&&(!a.contains(key)||!b.contains(key)||a.at(key)!=b.at(key)))return false;return true;};
 for(const auto& previous:pivots(book))if(owned(previous)&&previous.at("id")==generation.definition.at("id")&&sameExcept(previous,generation.definition,{"cache"})&&sameExcept(previous.at("cache"),generation.definition.at("cache"),{"sourceRevision","contextGeneration"})){
  std::map<std::pair<int,int>,const Json*> values;for(const auto& sheet:book.at("sheets"))for(const auto& cell:sheet.at("cells"))if(cell.value("pivotOwner",std::string())==previous.at("id"))values[{cell.at("row"),cell.at("col")}]=&cell;
  bool same=values.size()==generation.cells.size();for(const auto& cell:generation.cells){const auto found=values.find({cell.at("row"),cell.at("col")});if(found==values.end()||found->second->at("input")!=cell.at("input")){same=false;break;}}if(same)return Json::array();
 }
 return Json::array({{{"type","replace_pivot"},{"definition",generation.definition},{"cells",generation.cells}}});
}
Json deletePivotOperations(const Json& book,const std::string& id){for(const auto& pivot:pivots(book))if(owned(pivot)&&pivot.at("id")==id)return Json::array({{{"type","delete_pivot"},{"id",id}}});throw std::runtime_error("Editable pivot was not found");}
void applyPivotTransaction(Json& book,const Json& operation){
 const auto type=operation.at("type").get<std::string>();const bool remove=type=="delete_pivot";if(!remove&&type!="replace_pivot")throw std::runtime_error("Unknown pivot transaction");
 const auto id=remove?operation.at("id").get<std::string>():operation.at("definition").at("id").get<std::string>();
 if(remove)deletePivotOperations(book,id);else validateReplacement(book,operation.at("definition"),operation.at("cells"));
 if(!remove){
  const Json* previous=nullptr;for(const auto& pivot:pivots(book))if(owned(pivot)&&pivot.at("id")==id){previous=&pivot;break;}
  const auto& destination=operation.at("definition").at("output");
  if(previous){const auto& source=previous->at("output");if(source.at("sheet")!=destination.at("sheet")||source.at("row1")!=destination.at("row1")||source.at("col1")!=destination.at("col1")){
   std::string sourceName,destinationName;for(const auto& sheet:book.at("sheets")){if(sheet.at("id")==source.at("sheet"))sourceName=sheet.at("name");if(sheet.at("id")==destination.at("sheet"))destinationName=sheet.at("name");}
   if(sourceName.empty()||destinationName.empty())throw std::runtime_error("Pivot move worksheet was not found");
   const int dr=destination.at("row1").get<int>()-source.at("row1").get<int>(),dc=destination.at("col1").get<int>()-source.at("col1").get<int>();
   std::vector<std::pair<Json*,std::string>> rewrites;
   const auto rewrite=[&](Json& value,const std::string& ownSheet){const auto& formula=value.get_ref<const std::string&>();auto moved=moveFormulaReferences(formula,ownSheet,sourceName,destinationName,source.at("row1"),source.at("col1"),source.at("row2"),source.at("col2"),dr,dc);if(moved!=formula)rewrites.emplace_back(&value,std::move(moved));};
   for(auto& sheet:book["sheets"])for(auto& cell:sheet["cells"])if(cell.at("input").get_ref<const std::string&>().starts_with('='))rewrite(cell["input"],sheet.at("name"));
   if(book.contains("names"))for(auto& [name,value]:book["names"].items())if(value.is_string()){for(const auto& reference:formulaReferences(value.get_ref<const std::string&>()))if(reference.sheet.empty()&&!reference.external)throw std::runtime_error("Cannot safely move a report while workbook defined name "+name+" contains an unqualified reference; qualify its worksheet first");rewrite(value,{});}
   // Preflight every reference before changing formulas or removing any owned cells.
   for(auto& [value,formula]:rewrites)*value=std::move(formula);
  }}
 }
 std::map<std::pair<int,int>,Json> oldStyles;
 for(auto& sheet:book["sheets"]){auto& cells=sheet["cells"];for(const auto& cell:cells)if(cell.value("pivotOwner",std::string())==id&&cell.contains("style"))oldStyles[{cell.at("row"),cell.at("col")}]=cell.at("style");cells.erase(std::remove_if(cells.begin(),cells.end(),[&](const auto& cell){return cell.value("pivotOwner",std::string())==id;}),cells.end());}
 if(!book.contains("pivots"))book["pivots"]=Json::array();auto& definitions=book["pivots"];auto existing=std::find_if(definitions.begin(),definitions.end(),[&](const auto& pivot){return owned(pivot)&&pivot.at("id")==id;});
 if(remove){definitions.erase(existing);if(book.contains("charts")){auto& charts=book["charts"];charts.erase(std::remove_if(charts.begin(),charts.end(),[&](const auto& chart){return chart.value("pivotId",std::string())==id;}),charts.end());}return;}
 const auto& definition=operation.at("definition");if(existing==definitions.end())definitions.push_back(definition);else *existing=definition;
 for(auto& sheet:book["sheets"])if(sheet.at("id")==definition.at("output").at("sheet")){
  auto& cells=sheet["cells"];std::set<std::pair<int,int>> occupied;
  for(const auto& cell:operation.at("cells"))occupied.emplace(cell.at("row"),cell.at("col"));
  cells.erase(std::remove_if(cells.begin(),cells.end(),[&](const auto& cell){return occupied.contains({cell.at("row"),cell.at("col")});}),cells.end());
  for(auto cell:operation.at("cells")){auto found=oldStyles.find({cell.at("row"),cell.at("col")});if(found!=oldStyles.end()){auto style=found->second;if(cell.contains("style"))style.update(cell.at("style"));cell["style"]=std::move(style);}cells.push_back(std::move(cell));}
  std::sort(cells.begin(),cells.end(),[](const auto& a,const auto& b){return std::pair{a.at("row").template get<int>(),a.at("col").template get<int>()}<std::pair{b.at("row").template get<int>(),b.at("col").template get<int>()};});
 }
 if(book.contains("charts"))for(auto& chart:book["charts"])if(chart.value("pivotId",std::string())==id){chart["sheet"]=definition.at("output").at("sheet");auto range=definition.at("output");range.erase("sheet");chart["range"]=range;}
}
void validatePivotEdit(const Json& book,const Json& operation){
 const auto type=operation.at("type").get<std::string>();if(type=="replace_pivot"||type=="delete_pivot")return;
 if(type=="set_metadata"&&operation.at("key")=="pivots"){
  const auto nativeOnly=[](const Json& definitions){if(!definitions.is_array())throw std::runtime_error("Pivot metadata must be an array");Json result=Json::array();for(const auto& pivot:definitions)if(owned(pivot))result.push_back(pivot);return result;};
  if(nativeOnly(operation.at("value"))!=nativeOnly(pivots(book)))throw std::runtime_error("Native pivots require typed report operations; metadata replacement is unavailable");
 }
 for(const auto& pivot:pivots(book))if(owned(pivot)){
  const auto& output=pivot.at("output");
  if((type=="rename_sheet"||type=="delete_sheet")&&(operation.at("sheet")==output.at("sheet")||operation.at("sheet")==pivot.at("source").at("sheet")))throw std::runtime_error("Move or remove the report before renaming/deleting its source or output worksheet");
  if(!operation.contains("sheet")||operation.at("sheet")!=output.at("sheet")||type=="set_style")continue;
  if(type=="set_cell"&&inside(output,operation.at("row"),operation.at("col")))throw std::runtime_error("Pivot results are generated; edit the source or pivot fields instead");
  if(type=="clear_range"){auto range=operation;range["sheet"]=operation.at("sheet");if(overlaps(range,output))throw std::runtime_error("Cannot clear generated pivot result values");}
  if(type=="replace_sheet_cells"){
   std::map<std::pair<int,int>,const Json*> replacements;for(const auto& cell:operation.at("cells"))replacements[{cell.at("row"),cell.at("col")}]=&cell;
   for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==output.at("sheet"))for(const auto& cell:sheet.at("cells"))if(inside(output,cell.at("row"),cell.at("col"))){const auto found=replacements.find({cell.at("row"),cell.at("col")});if(found==replacements.end()||found->second->at("input")!=cell.at("input")||found->second->value("pivotOwner",std::string())!=pivot.at("id"))throw std::runtime_error("Structural editing cannot overwrite generated pivot values");}
  }
 }
}
void invalidatePivots(Json& book,const Json& operation){
 const auto type=operation.at("type").get<std::string>();if(type=="replace_pivot"||type=="delete_pivot"||!book.contains("pivots"))return;
 if(type=="set_style"&&!operation.value("replace",false)&&!operation.at("style").contains("numberFormat"))return;
 for(auto& pivot:book["pivots"])if(owned(pivot)){
  if(type=="set_metadata"&&operation.at("key")!="charts"&&operation.at("key")!="pivots"){pivot["stale"]=true;continue;}
  if(!operation.contains("sheet")||operation.at("sheet")!=pivot.at("source").at("sheet"))continue;
  if((type=="set_cell"||type=="set_style")&&!inside(pivot.at("source"),operation.at("row"),operation.at("col")))continue;
  if(type=="clear_range"&&!overlaps(operation,pivot.at("source")))continue;
  pivot["stale"]=true;
 }
}
void validatePivots(const Json& book){
 std::set<std::string> ids,names;
 for(const auto& pivot:pivots(book))if(owned(pivot)){
  if(!ids.insert(pivot.at("id").get<std::string>()).second||!names.insert(pivot.at("name").get<std::string>()).second)throw std::runtime_error("Duplicate native pivot identity or name");
  bounds(pivot.at("source"));bounds(pivot.at("output"));if(overlaps(pivot.at("source"),pivot.at("output")))throw std::runtime_error("Pivot output overlaps its source");
  if(!pivot.at("cache").at("fields").is_array()||!pivot.at("cache").at("records").is_array()||!pivot.at("values").is_array()||pivot.at("values").empty()||pivot.at("values").size()>4)throw std::runtime_error("Invalid native pivot cache or value fields");
 }
}
}
