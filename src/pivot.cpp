#include "tigersheets/pivot.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/formatting.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>
namespace tigersheets {
namespace {
using Address=std::uint64_t;
Address address(int row,int col){return (static_cast<std::uint64_t>(row)<<14)|static_cast<std::uint64_t>(col-1);}
Json typed(const Json& payload,const std::string& type){const auto canonical=type=="text"?"string":type=="empty"?"blank":type;return {{"type",canonical},{"value",canonical=="blank"?Json():payload}};}
Json blank(){return typed(nullptr,"blank");}
std::string display(const Json& value){if(value.at("type")=="blank")return "(blank)";if(value.at("value").is_string())return value.at("value").get<std::string>();return value.at("value").dump();}
std::string literal(const Json& value){if(value.at("type")=="blank")return "";if(value.at("type")=="string")return "'"+value.at("value").get<std::string>();if(value.at("type")=="boolean")return value.at("value").get<bool>()?"TRUE":"FALSE";return value.at("value").dump();}
const Json& sheetAt(const Json& book,const std::string& id){for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==id)return sheet;throw std::runtime_error("Pivot worksheet was not found");}
Json inputValue(const Json& cell){
 const auto input=cell.value("input",std::string());if(input.empty())return blank();
 if(input.front()=='\'')return typed(input.substr(1),"string");
 if(input=="TRUE"||input=="FALSE")return typed(input=="TRUE","boolean");
 double value{};auto parsed=std::from_chars(input.data(),input.data()+input.size(),value);
 if(parsed.ec==std::errc()&&parsed.ptr==input.data()+input.size()&&std::isfinite(value))return typed(value,"number");
 if(input.front()=='=')throw std::runtime_error("Pivot source formulas require current calculated values");
 return typed(input,"string");
}
void addCell(Json& cells,int row,int col,const Json& value,const std::string& owner,const std::string& format={}){
 Json cell={{"row",row},{"col",col},{"input",literal(value)},{"pivotOwner",owner}};
 if(!format.empty())cell["style"]={{"numberFormat",format}};
 cells.push_back(std::move(cell));
}
struct Aggregate {
 double sum=0,minimum=0,maximum=0;std::size_t numeric=0,nonempty=0;
 void add(const Json& value){if(value.at("type")!="blank")++nonempty;if(value.at("type")!="number")return;const auto number=value.at("value").get<double>();sum+=number;if(!numeric){minimum=maximum=number;}else{minimum=std::min(minimum,number);maximum=std::max(maximum,number);}++numeric;}
 Json result(const std::string& mode)const{if(mode=="count")return typed(nonempty,"number");if(mode=="sum")return typed(sum,"number");if(!numeric)return blank();if(mode=="average")return typed(sum/numeric,"number");if(mode=="min")return typed(minimum,"number");if(mode=="max")return typed(maximum,"number");throw std::runtime_error("Unsupported pivot aggregate: "+mode);}
};
Json formattedAxis(const Json& fields,const Json& axes,const Json& key){Json labels=Json::array();for(std::size_t i=0;i<key.size();++i){const auto& value=key[i];std::string format="General";if(axes[i].value("group",std::string()).empty())for(const auto& field:fields)if(field.at("name")==axes[i].at("field")){format=field.value("numberFormat",std::string("General"));break;}labels.push_back(value.at("type")=="blank"?"(blank)":formattedValue(value.at("value"),{{"numberFormat",format}}));}return labels;}
std::string keyLabel(const Json& labels){std::string label;for(const auto& value:labels){if(!label.empty())label+=" / ";label+=value.get<std::string>();}return label;}
Json groupedDate(const Json& value,const std::string& group){
 if(group.empty()||value.at("type")=="blank")return value;
 if(value.at("type")!="number")throw std::runtime_error("Date grouping requires numeric Excel date serials; text dates are ambiguous");
 const auto serial=std::floor(value.at("value").get<double>());if(serial<1||serial>2958465)throw std::runtime_error("Date grouping source is outside the 1900 date system");
 if(serial==60)throw std::runtime_error("The fictitious1900-02-29 date cannot be represented by native PivotTable date grouping");
 int year=1900,month=2;
 if(serial!=60){using namespace std::chrono;const year_month_day date{sys_days{std::chrono::year{1899}/December/31}+days{static_cast<int>(serial)-(serial>=60?1:0)}};year=int(date.year());month=unsigned(date.month());}
 if(group=="year")return typed(year,"number");if(group=="quarter")return typed((month-1)/3+1,"number");if(group=="month")return typed(month,"number");throw std::runtime_error("Unsupported pivot date grouping");
}
auto orderedMembers(const std::map<std::string,Json>& members){
 std::vector<std::pair<std::string,Json>> ordered(members.begin(),members.end());
 std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){for(std::size_t i=0;i<std::min(a.second.size(),b.second.size());++i){const auto& x=a.second[i];const auto& y=b.second[i];if(x==y)continue;if(x.at("type")==y.at("type"))return x.at("value")<y.at("value");return x.at("type")<y.at("type");}return a.second.size()<b.second.size();});return ordered;
}
}
PivotSourceCache buildPivotSourceCache(const Json& book,const Json& calculated,const Json& requested,const std::function<bool()>& cancelled){
 if(calculated.value("status",std::string())!="ok"||calculated.value("revision",Json())!=book.at("revision")||!calculated.value("converged",true)||(calculated.contains("iteration")&&!calculated["iteration"].value("converged",true)))throw std::runtime_error("Pivot requires current, converged calculation results");
 const auto& source=requested.at("source");const auto& sheet=sheetAt(book,source.at("sheet").get<std::string>());
 const int r1=source.at("row1"),r2=source.at("row2"),c1=source.at("col1"),c2=source.at("col2");
 if(r1<1||c1<1||r2>1048576||c2>16384||r2<=r1||c2<c1)throw std::runtime_error("Pivot source must contain a header and data within worksheet bounds");
 if(r2-r1>100000||static_cast<std::uint64_t>(r2-r1)*(c2-c1+1)>1000000)throw std::runtime_error("Pivot source exceeds 100,000 records or one million data cells");
 if(!requested.at("values").is_array()||requested.at("values").empty()||requested.at("values").size()>4)throw std::runtime_error("Choose between one and four pivot measures");
 if(book.value("settings",Json::object()).value("date1904",false))throw std::runtime_error("1904 date-system report calculation is unsupported");
 sheetAt(book,requested.at("output").at("sheet").get<std::string>());
 std::unordered_map<Address,const Json*> inputs,results;inputs.reserve(sheet.at("cells").size());
 for(const auto& cell:sheet.at("cells"))inputs[address(cell.at("row"),cell.at("col"))]=&cell;
 if(calculated.contains("values")){results.reserve(calculated.at("values").size());for(const auto& cell:calculated.at("values"))if(cell.at("sheet")==sheet.at("id"))results[address(cell.at("row"),cell.at("col"))]=&cell;}
 const auto valueAt=[&](int row,int col){const auto key=address(row,col);if(auto it=results.find(key);it!=results.end())return typed(it->second->at("value"),it->second->at("type"));if(auto it=inputs.find(key);it!=inputs.end())return inputValue(*it->second);return blank();};
 Json fields=Json::array(),records=Json::array();std::map<std::string,int> fieldIndex;
 std::set<std::string> headers;
 for(int col=c1;col<=c2;++col){const auto value=valueAt(r1,col);const auto name=display(value);auto normalized=name;for(auto& ch:normalized)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');if(value.at("type")!="string"||name.empty()||!headers.insert(normalized).second)throw std::runtime_error("Pivot headers must be unique nonempty text");fieldIndex[name]=col-c1;fields.push_back({{"name",name},{"numberFormat","General"},{"items",Json::array()}});}
 for(const auto& cell:sheet.at("cells"))if(cell.at("row")>r1&&cell.at("row")<=r2&&cell.at("col")>=c1&&cell.at("col")<=c2&&cell.contains("style")&&cell["style"].contains("numberFormat")){auto& format=fields[cell.at("col").get<int>()-c1]["numberFormat"];if(format=="General")format=cell["style"]["numberFormat"];}
 const std::set<std::string> aggregates{"sum","count","average","min","max"};
 for(const auto& measure:requested.at("values"))if(!fieldIndex.contains(measure.at("field").get<std::string>())||!aggregates.contains(measure.value("aggregate",std::string("sum"))))throw std::runtime_error("Choose an existing field and supported aggregate for each value");
 for(const auto* key:{"rows","columns","filters"})for(const auto& field:requested.value(key,Json::array()))if(!fieldIndex.contains(field.at("field").get<std::string>()))throw std::runtime_error("Pivot field is not present in its source: "+field.at("field").get<std::string>());
 std::vector<std::unordered_map<std::string,std::size_t>> dictionaries(fields.size()),literalIndexes(fields.size());
 records.get_ref<Json::array_t&>().reserve(r2-r1);
 std::size_t textBytes=0;
 for(int row=r1+1;row<=r2;++row){
  if(cancelled&&cancelled())throw std::runtime_error("Pivot refresh cancelled");
  Json record=Json::array();record.get_ref<Json::array_t&>().reserve(fields.size());
  for(int col=c1;col<=c2;++col){
   const auto keyAddress=address(row,col);const auto input=inputs.find(keyAddress);const std::string* raw=nullptr;static const std::string empty;
   if(input!=inputs.end()){const auto& text=input->second->at("input").get_ref<const std::string&>();if(!text.starts_with('='))raw=&text;}else if(!results.contains(keyAddress))raw=&empty;
   auto& repeated=literalIndexes[col-c1];
   if(raw){const auto seen=repeated.find(*raw);if(seen!=repeated.end()){const auto& existing=fields[col-c1]["items"][seen->second];if(existing.at("type")=="string"){textBytes+=existing.at("value").get_ref<const std::string&>().size();if(textBytes>128U*1024U*1024U)throw std::runtime_error("Pivot source exceeds 128 MiB decoded text budget");}record.push_back(seen->second);continue;}}
   auto value=valueAt(row,col);if(value.at("type")=="error")throw std::runtime_error("Pivot source contains a formula error at row "+std::to_string(row));
   if(value.at("type")=="string"){const auto& text=value.at("value").get_ref<const std::string&>();textBytes+=text.size();std::size_t length=0;for(unsigned char ch:text)length+=(ch&0xc0)!=0x80;if(length>32767||textBytes>128U*1024U*1024U)throw std::runtime_error("Pivot source exceeds the cell text limit or 128 MiB decoded text budget");}
   auto key=value.dump();auto& dictionary=dictionaries[col-c1];auto [it,added]=dictionary.emplace(key,dictionary.size());if(added)fields[col-c1]["items"].push_back(std::move(value));if(raw)repeated.emplace(*raw,it->second);record.push_back(it->second);
  }records.push_back(std::move(record));
 }
 return {std::move(fields),std::move(records)};
}
PivotGeneration generatePivot(const Json& book,const Json& calculated,const Json& requested,std::uint64_t context,const std::function<bool()>& cancelled){
 auto cache=buildPivotSourceCache(book,calculated,requested,cancelled);
 return generatePivotFromCache(book,requested,std::move(cache.fields),std::move(cache.records),context,cancelled);
}
PivotGeneration generatePivotFromCache(const Json& book,const Json& requested,Json fields,Json records,std::uint64_t context,const std::function<bool()>& cancelled){
 if(!fields.is_array()||fields.empty()||!records.is_array()||records.size()>100000||fields.size()*records.size()>1000000)throw std::runtime_error("Invalid or oversized pivot cache");
 sheetAt(book,requested.at("source").at("sheet").get<std::string>());sheetAt(book,requested.at("output").at("sheet").get<std::string>());
 if(!requested.at("values").is_array()||requested.at("values").empty()||requested.at("values").size()>4)throw std::runtime_error("Choose between one and four pivot measures");
 std::map<std::string,int> fieldIndex;std::set<std::string> headers;
 for(std::size_t i=0;i<fields.size();++i){const auto name=fields[i].at("name").get<std::string>();auto normalized=name;for(auto& ch:normalized)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch-'A'+'a');if(name.empty()||!headers.insert(normalized).second||!fields[i].at("items").is_array())throw std::runtime_error("Invalid pivot cache field");fieldIndex[name]=i;}
 const std::set<std::string> aggregates{"sum","count","average","min","max"};
 for(const auto& measure:requested.at("values"))if(!fieldIndex.contains(measure.at("field").get<std::string>())||!aggregates.contains(measure.value("aggregate",std::string("sum"))))throw std::runtime_error("Choose an existing field and supported aggregate for each value");
 for(const auto* key:{"rows","columns","filters"})for(const auto& field:requested.value(key,Json::array()))if(!fieldIndex.contains(field.at("field").get<std::string>()))throw std::runtime_error("Pivot field is not present in its source");
 std::vector<std::vector<std::size_t>> textSizes(fields.size());
 for(std::size_t f=0;f<fields.size();++f){for(const auto& item:fields[f].at("items")){const auto type=item.at("type").get<std::string>();const auto& value=item.at("value");std::size_t bytes=0;
  if(type=="number"){if(!value.is_number()||!std::isfinite(value.get<double>()))throw std::runtime_error("Pivot cache contains a nonfinite or invalid number");}
  else if(type=="string"){if(!value.is_string())throw std::runtime_error("Invalid pivot cache text");const auto& text=value.get_ref<const std::string&>();bytes=text.size();std::size_t length=0;for(unsigned char ch:text)length+=(ch&0xc0)!=0x80;if(length>32767)throw std::runtime_error("Pivot cache cell text exceeds 32767 characters");}
  else if(type=="blank"){if(!value.is_null())throw std::runtime_error("Invalid pivot cache blank");}
  else if(type=="boolean"){if(!value.is_boolean())throw std::runtime_error("Invalid pivot cache boolean");}
  else throw std::runtime_error("Unsupported or erroneous pivot cache value");
  textSizes[f].push_back(bytes);
 }}
 std::size_t textBytes=0;for(const auto& record:records){if(cancelled&&cancelled())throw std::runtime_error("Pivot refresh cancelled");if(!record.is_array()||record.size()!=fields.size())throw std::runtime_error("Pivot cache record width is invalid");for(std::size_t f=0;f<fields.size();++f){if(!record[f].is_number_unsigned()&&!record[f].is_number_integer())throw std::runtime_error("Invalid pivot cache index");const auto index=record[f].get<std::int64_t>();if(index<0||static_cast<std::uint64_t>(index)>=textSizes[f].size())throw std::runtime_error("Pivot cache index is outside its shared items");textBytes+=textSizes[f][index];if(textBytes>128U*1024U*1024U)throw std::runtime_error("Pivot source exceeds 128 MiB decoded text budget");}}
 const auto rowFields=requested.value("rows",Json::array()),columnFields=requested.value("columns",Json::array());const auto& measures=requested.at("values");
 auto member=[&](const Json& record,const std::string& field)->const Json&{const auto index=fieldIndex.at(field);return fields[index]["items"][record[index].get<std::size_t>()];};
 const auto filters=requested.value("filters",Json::array());
 auto included=[&](const Json& record){for(const auto& filter:filters){if(filter.value("all",true))continue;const auto& value=member(record,filter.at("field").get<std::string>());const auto selected=filter.value("items",Json::array());if(std::find(selected.begin(),selected.end(),value)==selected.end())return false;}return true;};
 auto dimensions=[&](const Json& record,const Json& axes){Json key=Json::array();for(const auto& axis:axes)key.push_back(groupedDate(member(record,axis.at("field").get<std::string>()),axis.value("group",std::string())));return key;};
 std::map<std::string,Json> rowMembers,columnMembers;
 using Buckets=std::vector<Aggregate>;std::map<std::pair<std::string,std::string>,Buckets> intersections;
 std::map<std::string,Buckets> rowAggregates,columnAggregates;Buckets grand(measures.size());
 for(const auto& record:records){
  if(cancelled&&cancelled())throw std::runtime_error("Pivot refresh cancelled");
  if(!included(record))continue;
  auto rowKey=dimensions(record,rowFields),columnKey=dimensions(record,columnFields);auto r=rowKey.dump(),c=columnKey.dump();rowMembers[r]=rowKey;columnMembers[c]=columnKey;
  auto& bucket=intersections[{r,c}];auto& rowBucket=rowAggregates[r];auto& columnBucket=columnAggregates[c];bucket.resize(measures.size());rowBucket.resize(measures.size());columnBucket.resize(measures.size());
  for(std::size_t i=0;i<measures.size();++i){const auto& value=member(record,measures[i].at("field").get<std::string>());bucket[i].add(value);rowBucket[i].add(value);columnBucket[i].add(value);grand[i].add(value);}
 }
 if(rowFields.empty()&&rowMembers.empty())rowMembers["[]"]=Json::array();if(columnFields.empty()&&columnMembers.empty())columnMembers["[]"]=Json::array();
 const auto orderedRows=orderedMembers(rowMembers),orderedColumns=orderedMembers(columnMembers);
 Json definition=Json::object();for(const auto& [key,value]:requested.items())if(key!="cache"&&key!="layout")definition[key]=value;definition["version"]=1;definition["stale"]=false;definition["rows"]=rowFields;definition["columns"]=columnFields;
 definition["filters"]=filters;for(auto& filter:definition["filters"]){filter["missingItems"]=Json::array();const auto& available=fields[fieldIndex.at(filter.at("field").get<std::string>())]["items"];for(const auto& selected:filter.value("items",Json::array()))if(std::find(available.begin(),available.end(),selected)==available.end())filter["missingItems"].push_back(selected);}
 definition["rowGrandTotals"]=requested.value("rowGrandTotals",true);definition["columnGrandTotals"]=requested.value("columnGrandTotals",true);
 Json cells=Json::array(),rowKeys=Json::array(),columnKeys=Json::array(),data=Json::array(),rowTotals=Json::array(),columnTotals=Json::array(),grandTotals=Json::array();
 const auto id=definition.at("id").get<std::string>();const auto& output=definition.at("output");const int top=output.at("row1"),left=output.at("col1");const int labels=static_cast<int>(rowFields.size());
 const bool rightTotals=!columnFields.empty()&&definition["rowGrandTotals"].get<bool>(),bottomTotals=!rowFields.empty()&&definition["columnGrandTotals"].get<bool>();
 const auto width=labels+(columnMembers.size()+(rightTotals?1:0))*measures.size(),height=rowMembers.size()+1+(bottomTotals?1:0);
 if(width*height>250000||top<1||left<1||static_cast<std::uint64_t>(top)+height-1>1048576||static_cast<std::uint64_t>(left)+width-1>16384)throw std::runtime_error("Projected pivot layout "+std::to_string(height)+" rows × "+std::to_string(width)+" columns exceeds 250,000 cells or worksheet bounds; filter members or remove fields");
 auto result=[&](const Buckets& bucket,std::size_t i){return (bucket.size()>i?bucket[i]:Aggregate{}).result(measures[i].value("aggregate",std::string("sum")));};
 auto format=[&](std::size_t i){return measures[i].value("numberFormat",std::string("General"));};
 for(int i=0;i<labels;++i)addCell(cells,top,left+i,typed(rowFields[i].at("field"),"string"),id);
 int col=left+labels;
 for(const auto& [key,members]:orderedColumns){columnKeys.push_back(members);for(std::size_t i=0;i<measures.size();++i){auto title=measures[i].value("name",measures[i].at("field").get<std::string>());if(!members.empty())title=keyLabel(formattedAxis(fields,columnFields,members))+" / "+title;addCell(cells,top,col++,typed(title,"string"),id);}}
 if(rightTotals)for(const auto& measure:measures)addCell(cells,top,col++,typed("Grand Total / "+measure.value("name",measure.at("field").get<std::string>()),"string"),id);
 int row=top+1;
 for(const auto& [r,members]:orderedRows){
  rowKeys.push_back(members);for(int i=0;i<labels;++i)addCell(cells,row,left+i,members[i],id,rowFields[i].value("group",std::string()).empty()?fields[fieldIndex.at(rowFields[i].at("field").get<std::string>())].value("numberFormat",std::string("General")):std::string("General"));Json line=Json::array(),totals=Json::array();int out=left+labels;
  for(const auto& [c,key]:orderedColumns){const auto found=intersections.find({r,c});const auto bucket=found==intersections.end()?Buckets{}:found->second;for(std::size_t i=0;i<measures.size();++i){auto value=result(bucket,i);line.push_back(value);addCell(cells,row,out++,value,id,format(i));}}
  for(std::size_t i=0;i<measures.size();++i){auto value=result(rowAggregates[r],i);totals.push_back(value);if(rightTotals)addCell(cells,row,out++,value,id,format(i));}
  rowTotals.push_back(std::move(totals));data.push_back(std::move(line));++row;
 }
 for(std::size_t i=0;i<measures.size();++i)grandTotals.push_back(result(grand,i));
 for(const auto& [c,key]:orderedColumns)for(std::size_t i=0;i<measures.size();++i)columnTotals.push_back(result(columnAggregates[c],i));
 if(bottomTotals){for(int i=0;i<labels;++i)addCell(cells,row,left+i,i?blank():typed("Grand Total","string"),id);int out=left+labels;for(std::size_t i=0;i<columnTotals.size();++i)addCell(cells,row,out++,columnTotals[i],id,format(i%measures.size()));if(rightTotals)for(std::size_t i=0;i<grandTotals.size();++i)addCell(cells,row,out++,grandTotals[i],id,format(i));++row;}
 definition["layout"]={{"rowKeys",std::move(rowKeys)},{"columnKeys",std::move(columnKeys)},{"data",std::move(data)},{"rowTotals",std::move(rowTotals)},{"columnTotals",std::move(columnTotals)},{"grandTotals",std::move(grandTotals)}};
 definition["cache"]=Json::object();definition["cache"]["fields"]=std::move(fields);definition["cache"]["records"]=std::move(records);definition["cache"]["sourceRevision"]=book.at("revision");definition["cache"]["contextGeneration"]=context;
 definition["output"]["row2"]=row-1;definition["output"]["col2"]=col-1;
 return {std::move(definition),std::move(cells)};
}
namespace {
using GroupKey=std::pair<std::string,std::string>;
// Match the native cache-field names, including suffixes when source headers collide.
std::map<GroupKey,std::string> groupedAliases(const Json& pivot){
 std::map<GroupKey,std::string> requested,aliases;std::set<std::string> occupied;
 for(const auto& field:pivot.at("cache").at("fields"))occupied.insert(field.at("name").get<std::string>());
 for(const auto* key:{"rows","columns","filters"})for(const auto& axis:pivot.value(key,Json::array()))if(!axis.value("group",std::string()).empty())requested[{axis.at("field").get<std::string>(),axis.at("group").get<std::string>()}]=axis.value("nativeName",std::string());
 for(const auto& field:pivot.at("cache").at("fields"))for(const auto* group:{"month","quarter","year"}){
  const GroupKey key{field.at("name").get<std::string>(),group};const auto found=requested.find(key);if(found==requested.end())continue;
  auto name=found->second.empty()?key.first+" ("+key.second+")":found->second;while(occupied.contains(name))name+='_';occupied.insert(name);aliases[key]=std::move(name);
 }
 return aliases;
}
}
Json getPivotData(const Json& book,const std::string& sheet,int row,int col,const std::string& measure,const Json& fieldItems){
 const auto error=[](){return typed("#REF!","error");};
 try{
  if(!fieldItems.is_array())return error();const Json* selected=nullptr;
  if(book.contains("pivots"))for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1){const auto& output=pivot.at("output");if(output.at("sheet")==sheet&&row>=output.at("row1")&&row<=output.at("row2")&&col>=output.at("col1")&&col<=output.at("col2"))selected=&pivot;}
  if(!selected)return error();const auto& pivot=*selected;const Json* selectedMeasure=nullptr;
  for(const auto& item:pivot.at("values"))if(item.value("name",item.at("field").get<std::string>())==measure){selectedMeasure=&item;break;}
  if(!selectedMeasure)for(const auto& item:pivot.at("values"))if(item.at("field")==measure){selectedMeasure=&item;break;}
  if(!selectedMeasure)return error();const auto& fields=pivot.at("cache").at("fields");std::map<std::string,std::size_t> indexes;
  for(std::size_t i=0;i<fields.size();++i)indexes[fields[i].at("name").get<std::string>()]=i;
  std::map<std::string,GroupKey> visible;const auto aliases=groupedAliases(pivot);
  for(const auto* key:{"rows","columns","filters"})for(const auto& axis:pivot.value(key,Json::array())){const auto field=axis.at("field").get<std::string>();visible[field]={field,{}};const auto group=axis.value("group",std::string());if(!group.empty())visible[aliases.at({field,group})]={field,group};}
  for(const auto& pair:fieldItems)if(!visible.contains(pair.at("field").get<std::string>()))return error();
  const auto sourceValue=[&](const Json& record,const std::string& name)->const Json&{const auto index=indexes.at(name);return fields[index].at("items")[record[index].get<std::size_t>()];};
  Aggregate aggregate;std::size_t matches=0;
  for(const auto& record:pivot.at("cache").at("records")){
   bool include=true;
   for(const auto& filter:pivot.value("filters",Json::array()))if(!filter.value("all",true)){const auto& value=sourceValue(record,filter.at("field").get<std::string>());const auto items=filter.value("items",Json::array());if(std::find(items.begin(),items.end(),value)==items.end()){include=false;break;}}
   if(!include)continue;
   for(const auto& pair:fieldItems){const auto& expected=pair.at("item");const auto& axis=visible.at(pair.at("field").get<std::string>());const auto value=groupedDate(sourceValue(record,axis.first),axis.second);if(value!=expected){include=false;break;}}
   if(!include)continue;++matches;aggregate.add(sourceValue(record,selectedMeasure->at("field").get<std::string>()));
  }
  if(!matches&&!fieldItems.empty())return error();return aggregate.result(selectedMeasure->value("aggregate",std::string("sum")));
 }catch(const std::exception&){return error();}
}
std::string pivotReferenceFormula(const Json& book,const std::string& sheet,int row,int col){
 try{
  if(!book.contains("pivots"))return {};
  const auto quote=[](const std::string& text){std::string out="\"";for(char ch:text){out+=ch;if(ch=='\"')out+=ch;}return out+'\"';};
  const auto item=[&](const Json& value){const auto type=value.at("type").get<std::string>();if(type=="blank")return std::string();if(type=="string")return quote(value.at("value").get<std::string>());if(type=="boolean")return std::string(value.at("value").get<bool>()?"TRUE":"FALSE");if(type=="number"&&value.at("value").is_number())return value.at("value").dump();throw std::runtime_error("Unsupported pivot formula member");};
  for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1){
   const auto& output=pivot.at("output");if(output.at("sheet")!=sheet||row<=output.at("row1")||row>output.at("row2")||col<output.at("col1")||col>output.at("col2"))continue;
   const auto& rows=pivot.at("rows");const auto& columns=pivot.at("columns");const auto& measures=pivot.at("values");const auto& layout=pivot.at("layout");const auto& rowKeys=layout.at("rowKeys");const auto& columnKeys=layout.at("columnKeys");
   const int rowIndex=row-output.at("row1").get<int>()-1,columnOffset=col-output.at("col1").get<int>()-static_cast<int>(rows.size());if(columnOffset<0||measures.empty())return {};
   const auto measureCount=static_cast<int>(measures.size()),ordinaryColumns=static_cast<int>(columnKeys.size())*measureCount;
   const bool bottom=rowIndex==static_cast<int>(rowKeys.size())&&!rows.empty()&&pivot.value("columnGrandTotals",true);
   const bool right=columnOffset>=ordinaryColumns&&columnOffset<ordinaryColumns+measureCount&&!columns.empty()&&pivot.value("rowGrandTotals",true);
   if((!bottom&&(rowIndex<0||rowIndex>=static_cast<int>(rowKeys.size())))||(!right&&columnOffset>=ordinaryColumns))return {};
   const auto& measure=measures[columnOffset%measureCount];std::string sheetName="'";for(char ch:sheetAt(book,sheet).at("name").get<std::string>()){sheetName+=ch;if(ch=='\'')sheetName+=ch;}sheetName+="'!";
   std::string formula="=GETPIVOTDATA("+quote(measure.value("name",measure.at("field").get<std::string>()))+","+sheetName+"$"+columnName(output.at("col1"))+"$"+std::to_string(output.at("row1").get<int>());
   const auto aliases=groupedAliases(pivot);
   const auto append=[&](const Json& axes,const Json& members){for(std::size_t i=0;i<axes.size();++i){const auto field=axes[i].at("field").get<std::string>(),group=axes[i].value("group",std::string());formula+=","+quote(group.empty()?field:aliases.at({field,group}))+","+item(members.at(i));}};
   if(!bottom)append(rows,rowKeys.at(rowIndex));if(!right)append(columns,columnKeys.at(columnOffset/measureCount));return formula+")";
  }
 }catch(const std::exception&){}
 return {};
}
Json pivotChartData(const Json& book,const Json& chart){
 if(!book.contains("pivots"))throw std::runtime_error("PivotChart source was not found");const Json* selected=nullptr;for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1&&pivot.at("id")==chart.at("pivotId")){selected=&pivot;break;}
 if(!selected)throw std::runtime_error("PivotChart source was not found");const auto& pivot=*selected;const auto& layout=pivot.at("layout");const auto& rows=layout.at("rowKeys");const auto& columns=layout.at("columnKeys");const auto& measures=pivot.at("values");
 if(rows.size()>1000||columns.size()*measures.size()>16)throw std::runtime_error("PivotChart exceeds 1,000 categories or sixteen series; filter the chart's pivot fields. The complete table is retained.");
 Json categories=Json::array(),categoryLevels=Json::array(),series=Json::array();for(const auto& row:rows){auto levels=formattedAxis(pivot.at("cache").at("fields"),pivot.at("rows"),row);const auto label=keyLabel(levels);categories.push_back(label.empty()?"Total":label);categoryLevels.push_back(std::move(levels));}
 for(std::size_t c=0;c<columns.size();++c)for(std::size_t m=0;m<measures.size();++m){auto name=measures[m].value("name",measures[m].at("field").get<std::string>());const auto prefix=keyLabel(formattedAxis(pivot.at("cache").at("fields"),pivot.at("columns"),columns[c]));if(!prefix.empty())name=prefix+" / "+name;Json values=Json::array();for(std::size_t r=0;r<rows.size();++r){const auto& value=layout.at("data")[r][c*measures.size()+m];values.push_back(value.at("type")=="number"?value.at("value"):Json());}series.push_back({{"name",name},{"values",std::move(values)},{"numberFormat",measures[m].value("numberFormat",std::string("General"))}});}
 return {{"categories",std::move(categories)},{"categoryLevels",std::move(categoryLevels)},{"series",std::move(series)},{"pivotId",pivot.at("id")},{"stale",pivot.value("stale",false)}};
}
Json pivotChart(const Json& book,const std::string& pivotId,const std::string& chartId,const std::string& type){
 if(type!="line"&&type!="column"&&type!="bar")throw std::runtime_error("PivotCharts support line, column and bar families");if(chartId.empty())throw std::runtime_error("PivotChart requires a stable id");
 if(book.contains("charts"))for(const auto& chart:book.at("charts"))if(chart.value("id",std::string())==chartId)throw std::runtime_error("Chart id already exists");
 const Json* selected=nullptr;if(book.contains("pivots"))for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1&&pivot.at("id")==pivotId){selected=&pivot;break;}if(!selected)throw std::runtime_error("Editable pivot was not found");const auto& pivot=*selected;auto range=pivot.at("output");range.erase("sheet");
 Json chart={{"id",chartId},{"pivotId",pivotId},{"sheet",pivot.at("output").at("sheet")},{"type",type},{"title",pivot.at("name")},{"range",range},{"anchor",{{"row",std::min(1048576,pivot.at("output").at("row2").get<int>()+2)},{"col",pivot.at("output").at("col1")},{"width",560},{"height",300}}}};pivotChartData(book,chart);return chart;
}
}
