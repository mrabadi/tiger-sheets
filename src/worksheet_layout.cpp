#include "tigersheets/worksheet_layout.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace tigersheets {
namespace {
void validColumnWidth(double value,int maxDigitWidth){
 if(!std::isfinite(value)||value<0||value>255)throw std::invalid_argument("Column width must be finite and between 0 and 255");
 if(maxDigitWidth<=0)throw std::invalid_argument("Maximum digit width must be positive");
}
}
// ISO/IEC29500 col width equations, with the documented 8-character/61px example.
double spreadsheetColumnWidth(double characters,int maxDigitWidth){
 validColumnWidth(characters,maxDigitWidth);if(characters==0)return 0;
 return std::min(255.0,std::floor((characters*maxDigitWidth+5.0)/maxDigitWidth*256.0)/256.0);
}
int spreadsheetColumnPixels(double storedWidth,int maxDigitWidth){
 validColumnWidth(storedWidth,maxDigitWidth);
 const auto pixels=std::floor(((256.0*storedWidth+std::floor(128.0/maxDigitWidth))/256.0)*maxDigitWidth);
 if(pixels>std::numeric_limits<int>::max())throw std::overflow_error("Column pixel width exceeds the supported integer range");
 return static_cast<int>(pixels);
}
double spreadsheetColumnCharacters(double storedWidth,int maxDigitWidth){
 return std::max(0.0,std::floor((spreadsheetColumnPixels(storedWidth,maxDigitWidth)-5.0)/maxDigitWidth*100.0+0.5)/100.0);
}
namespace {
int integer(const Json& object,const char* key,int low,int high){if(!object.contains(key)||!object.at(key).is_number_integer())throw std::runtime_error(std::string("Invalid layout ")+key);const auto value=object.at(key).get<std::int64_t>();if(value<low||value>high)throw std::runtime_error(std::string("Layout ")+key+" is outside Excel limits");return static_cast<int>(value);}
void fields(const Json& object,const std::set<std::string>& allowed){if(!object.is_object())throw std::runtime_error("Layout entry must be an object");for(const auto& [key,value]:object.items())if(!allowed.contains(key))throw std::runtime_error("Unknown layout property: "+key);}
void dimensions(const Json& sheet,const char* key,int limit,double maximum){if(!sheet.contains(key))return;const auto& values=sheet.at(key);if(!values.is_array()||values.size()>static_cast<std::size_t>(limit))throw std::runtime_error("Invalid worksheet dimension list");std::set<int> indices;for(const auto& entry:values){fields(entry,{"index","size","hidden"});const auto index=integer(entry,"index",1,limit);if(!indices.insert(index).second)throw std::runtime_error("Duplicate worksheet dimension");if(!entry.contains("size")&&!entry.contains("hidden"))throw std::runtime_error("Worksheet dimension requires size or hidden");if(entry.contains("size")){if(!entry.at("size").is_number())throw std::runtime_error("Dimension size must be numeric");const auto value=entry.at("size").get<double>();if(!std::isfinite(value)||value<=0||value>maximum)throw std::runtime_error("Dimension size is outside Excel limits");}if(entry.contains("hidden")&&!entry.at("hidden").is_boolean())throw std::runtime_error("Dimension hidden must be a boolean");}}
}
namespace {
bool overlap(const Json& a,const Json& b){return a.at("row1")<=b.at("row2")&&b.at("row1")<=a.at("row2")&&a.at("col1")<=b.at("col2")&&b.at("col1")<=a.at("col2");}
bool referencesRange(const Json& value,const std::string& sheet,const Json& range,const std::string& inherited={}){
 if(value.is_object()){
  const auto scope=value.value("sheet",inherited);if(scope==sheet){
   if(value.contains("row1")&&value.contains("row2")&&value.contains("col1")&&value.contains("col2")&&overlap(value,range))return true;
   if(value.contains("row")&&value.contains("col")&&value.at("row")>=range.at("row1")&&value.at("row")<=range.at("row2")&&value.at("col")>=range.at("col1")&&value.at("col")<=range.at("col2"))return true;
  }
  for(const auto& [key,item]:value.items())if(key!="cache"&&key!="results"&&referencesRange(item,sheet,range,scope))return true;
 }else if(value.is_array())for(const auto& item:value)if(referencesRange(item,sheet,range,inherited))return true;
 return false;
}
}
void validateWorksheetLayout(const Json& workbook,const Json& sheet){
 dimensions(sheet,"rowLayout",1048576,409.5);dimensions(sheet,"columnLayout",16384,255.0);
 if(!sheet.contains("mergedRanges"))return;const auto& merges=sheet.at("mergedRanges");if(!merges.is_array()||merges.size()>10000)throw std::runtime_error("Merged ranges must be an array of at most 10,000 rectangles");
 for(const auto& range:merges){fields(range,{"row1","col1","row2","col2"});const int r1=integer(range,"row1",1,1048576),r2=integer(range,"row2",r1,1048576),c1=integer(range,"col1",1,16384),c2=integer(range,"col2",c1,16384);if(r1==r2&&c1==c2)throw std::runtime_error("Merge requires at least two cells");}
 for(const auto& range:merges){for(const auto* collection:{"pivots","financialModels","tables"})if(workbook.contains(collection)&&referencesRange(workbook.at(collection),sheet.at("id"),range))throw std::runtime_error("Cannot merge cells used by a report or table; move the report first");for(const auto& cell:sheet.at("cells"))if((cell.contains("pivotOwner")||cell.contains("financialModelOwner"))&&cell.at("row")>=range.at("row1")&&cell.at("row")<=range.at("row2")&&cell.at("col")>=range.at("col1")&&cell.at("col")<=range.at("col2"))throw std::runtime_error("Cannot merge generated report cells");}
 for(std::size_t i=0;i<merges.size();++i)for(std::size_t j=0;j<i;++j){const auto& a=merges[i];const auto& b=merges[j];if(a.at("row1")<=b.at("row2")&&b.at("row1")<=a.at("row2")&&a.at("col1")<=b.at("col2")&&b.at("col1")<=a.at("col2"))throw std::runtime_error("Merged ranges cannot overlap");}
}
void rebaseWorksheetLayout(Json& sheet,bool rows,int index,int count,bool inserting){
 const int limit=rows?1048576:16384;if(index<1||index>limit||count<1||count>limit-index+1)throw std::runtime_error("Invalid worksheet layout structural edit");auto candidate=sheet;const char* first=rows?"row1":"col1",*last=rows?"row2":"col2";
 const auto shift=[&](int value){const auto result=static_cast<std::int64_t>(value)+(value>=index?count:0);if(result>limit)throw std::runtime_error("Worksheet dimension would exceed Excel limits");return static_cast<int>(result);};
 if(candidate.contains("mergedRanges")){Json next=Json::array();for(auto range:candidate.at("mergedRanges")){int low=range.at(first),high=range.at(last);if(inserting){low=shift(low);high=shift(high);}else{const int end=index+count-1;if(high<index){}else if(low>end){low-=count;high-=count;}else{if(low>=index)low=index;if(high<=end)high=index-1;else high-=count;if(high<low)continue;}}range[first]=low;range[last]=high;if(range.at("row1")==range.at("row2")&&range.at("col1")==range.at("col2"))continue;next.push_back(std::move(range));}candidate["mergedRanges"]=std::move(next);}
 const char* dimensions=rows?"rowLayout":"columnLayout";if(candidate.contains(dimensions)){Json next=Json::array();for(auto entry:candidate.at(dimensions)){int at=entry.at("index");if(inserting)at=shift(at);else if(at>=index&&at<index+count)continue;else if(at>=index+count)at-=count;entry["index"]=at;next.push_back(std::move(entry));}candidate[dimensions]=std::move(next);}
 const char* hidden=rows?"hiddenRows":"hiddenColumns";if(candidate.contains(hidden)){Json next=Json::array();for(const auto& entry:candidate.at(hidden)){int at=entry.get<int>();if(inserting)at=shift(at);else if(at>=index&&at<index+count)continue;else if(at>=index+count)at-=count;next.push_back(at);}candidate[hidden]=std::move(next);}
 sheet=std::move(candidate);
}
void applyWorksheetLayout(Json& workbook,const Json& operation){
 fields(operation,{"type","sheet","mergedRanges","rows","columns"});if(operation.value("type",std::string())!="set_sheet_layout")throw std::runtime_error("Unknown worksheet layout operation");
 auto found=std::find_if(workbook["sheets"].begin(),workbook["sheets"].end(),[&](const Json& sheet){return sheet.at("id")==operation.at("sheet");});if(found==workbook["sheets"].end())throw std::runtime_error("Worksheet was not found");
 auto candidate=*found;for(const auto& [external,internal]:{std::pair{"mergedRanges","mergedRanges"},std::pair{"rows","rowLayout"},std::pair{"columns","columnLayout"}})if(operation.contains(external))candidate[internal]=operation.at(external);
 if(operation.contains("rows")){candidate["hiddenRows"]=Json::array();for(const auto& row:candidate.at("rowLayout"))if(row.value("hidden",false))candidate["hiddenRows"].push_back(row.at("index"));}
 if(operation.contains("columns")){candidate["hiddenColumns"]=Json::array();for(const auto& col:candidate.at("columnLayout"))if(col.value("hidden",false))candidate["hiddenColumns"].push_back(col.at("index"));}
 validateWorksheetLayout(workbook,candidate);const bool hiddenRowsChanged=found->value("hiddenRows",Json::array())!=candidate.value("hiddenRows",Json::array());*found=std::move(candidate);
 // SUBTOTAL can feed a report through formulas on another sheet. Until the
 // complete formula dependency graph is available here, invalidation is global.
 if(hiddenRowsChanged&&workbook.contains("pivots"))for(auto& pivot:workbook["pivots"])if(pivot.is_object()&&pivot.value("version",0)==1)pivot["stale"]=true;
}
}
