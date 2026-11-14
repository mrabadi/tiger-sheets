#include "prepared_native_transaction.hpp"
#include <charconv>

namespace tigersheets {
namespace {
bool native(const Json& pivot){return pivot.is_object()&&pivot.value("version",0)==1;}
bool sameAnchor(const Json& a,const Json& b){return a.at("sheet")==b.at("sheet")&&a.at("row1")==b.at("row1")&&a.at("col1")==b.at("col1");}
}
NativePreparationProjection::NativePreparationProjection(Json& workbook,const Json& operations):workbook_(workbook){
 if(!operations.is_array()||operations.empty()||operations.back().value("type",std::string())!="replace_pivot")return;
 for(std::size_t index=0;index+1<operations.size();++index){const auto& operation=operations[index];const auto type=operation.value("type",std::string());if(type!="add_sheet"&&(type!="set_metadata"||operation.at("key")!="pivots"))return;}
 const auto& definition=operations.back().at("definition");const auto& id=definition.at("id");const auto& output=definition.at("output");
 if(workbook.contains("pivots"))for(const auto& previous:workbook.at("pivots"))if(native(previous)&&previous.at("id")==id&&!sameAnchor(previous.at("output"),output))return;
 // A replacement removes every old owner cell, even outside its output. Keep
 // those workbooks on the general path rather than hiding a removable cell.
 for(const auto& sheet:workbook.at("sheets"))if(sheet.at("id")!=output.at("sheet"))for(const auto& cell:sheet.at("cells")){const auto owner=cell.find("pivotOwner");if(owner!=cell.end()&&(!owner->is_string()||*owner==id))return;}
 std::set<std::string> preserved;for(const auto& sheet:workbook.at("sheets"))if(sheet.at("id")!=output.at("sheet"))preserved.insert(sheet.at("id").get<std::string>());detach(preserved);
}
NativePreparationProjection::NativePreparationProjection(Json& workbook,const Json& changes,HistoryPatch):workbook_(workbook){
 if(!changes.is_array())return;
 std::vector<std::string> order;std::set<std::string> preserved;
 for(const auto& sheet:workbook.at("sheets")){const auto id=sheet.at("id").get<std::string>();order.push_back(id);preserved.insert(id);}
 bool nativeGraph=false,structural=false;
 if(workbook.contains("pivots"))for(const auto& pivot:workbook.at("pivots"))nativeGraph|=native(pivot);
 for(const auto& change:changes){
  const auto& operation=change.at("op").get_ref<const std::string&>();const auto& path=change.at("path").get_ref<const std::string&>();
  if(operation!="add"&&operation!="remove"&&operation!="replace")return;
  if(path.empty()||path=="/sheets")return;
  if(path=="/pivots"&&change.contains("value")&&change.at("value").is_array())for(const auto& pivot:change.at("value"))nativeGraph|=native(pivot);
  if(path.starts_with("/pivots/")&&change.contains("value"))nativeGraph|=native(change.at("value"));
  if(!path.starts_with("/sheets/"))continue;
  // Only whole worksheet insertion/removal is eligible. Any interior sheet
  // edit, reorder primitive or unknown pointer retains the general patch path.
  const auto segment=std::string_view(path).substr(8);if(segment.empty()||segment.find('/')!=std::string_view::npos)return;
  std::size_t index=order.size();if(segment!="-"){const auto parsed=std::from_chars(segment.data(),segment.data()+segment.size(),index);if(parsed.ec!=std::errc()||parsed.ptr!=segment.data()+segment.size())return;}
  if(operation=="add"){
   if(index>order.size()||!change.at("value").is_object()||!change.at("value").contains("id")||!change.at("value").at("id").is_string())return;
   auto id=change.at("value").at("id").get<std::string>();preserved.erase(id);order.insert(order.begin()+index,std::move(id));
  }else{
   if(index>=order.size())return;preserved.erase(order[index]);
   if(operation=="remove")order.erase(order.begin()+index);
   else{if(!change.at("value").is_object()||!change.at("value").contains("id")||!change.at("value").at("id").is_string())return;auto id=change.at("value").at("id").get<std::string>();preserved.erase(id);order[index]=std::move(id);}
  }
  structural=true;
 }
 if(nativeGraph&&structural){
  // Removed/replaced top-level metadata need not be cloned merely to discard
  // it. Keep its original allocation for rollback until the patch succeeds.
  // Nested or repeated edits to that field keep the general representation.
  for(const auto& change:changes){const auto& path=change.at("path").get_ref<const std::string&>();const auto& operation=change.at("op").get_ref<const std::string&>();if(operation!="remove"&&operation!="replace")continue;
   if(path.size()<2||path.find('/',1)!=std::string::npos||path.find('~')!=std::string::npos||path=="/revision"||path=="/sheets")continue;const auto key=path.substr(1);if(!workbook.contains(key))continue;
   std::size_t edits=0;for(const auto& other:changes){const auto& target=other.at("path").get_ref<const std::string&>();edits+=target==path||target.starts_with(path+"/");}if(edits==1)metadata_.emplace_back(key,Json());
  }
  detach(preserved);
  for(auto& [key,value]:metadata_)workbook_.at(key).swap(value);
 }
}
void NativePreparationProjection::detach(const std::set<std::string>& preserved){
 cells_.reserve(preserved.size());
 // Allocate every restoration slot before detaching anything. The swaps below
 // cannot throw, so partially constructed guards never strand a source array.
 for(const auto& sheet:workbook_.at("sheets"))if(preserved.contains(sheet.at("id").get_ref<const std::string&>()))cells_.emplace_back(sheet.at("id").get<std::string>(),Json::array());
 std::size_t index=0;for(auto& sheet:workbook_["sheets"])if(preserved.contains(sheet.at("id").get_ref<const std::string&>()))sheet["cells"].swap(cells_[index++].second);
}
NativePreparationProjection::~NativePreparationProjection(){restore();}
void NativePreparationProjection::restore(bool appliedHistory) noexcept{
 for(auto& [id,cells]:cells_)for(auto& sheet:workbook_["sheets"])if(sheet.at("id").get_ref<const std::string&>()==id){sheet["cells"].swap(cells);break;}
 if(!appliedHistory)for(auto& [key,value]:metadata_)workbook_.at(key).swap(value);
 cells_.clear();
 metadata_.clear();
}
}
