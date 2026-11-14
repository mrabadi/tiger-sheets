#include "tigersheets/session.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/pivot_ownership.hpp"
#include "tigersheets/financial_models.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/worksheet_validation.hpp"
#include "prepared_native_transaction.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace tigersheets {
namespace {
constexpr int maxRows=1048576,maxColumns=16384;
void address(int row,int col){if(row<1||row>maxRows||col<1||col>maxColumns)throw std::runtime_error("Cell address is out of bounds");}
std::string lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return text;}
void sheetName(const std::string& name){for(unsigned char c:name)if(c<32||c==127)throw std::runtime_error("Worksheet names cannot contain control characters");if(name.empty()||name.size()>124||name.find_first_of("[]:*?/\\")!=std::string::npos||name.front()=='\''||name.back()=='\'')throw std::runtime_error("Invalid worksheet name");std::size_t count=0;for(const auto c:name)if((static_cast<unsigned char>(c)&0xc0U)!=0x80U)++count;if(count>31)throw std::runtime_error("Worksheet names are limited to 31 characters");}
int metadataInteger(const Json& value,int low,int high,const std::string& field){
 if(!value.is_number_integer())throw std::runtime_error(field+" must be an integer");
 if(value.is_number_unsigned()){
  const auto number=value.get<std::uint64_t>();if(number<static_cast<std::uint64_t>(low)||number>static_cast<std::uint64_t>(high))throw std::runtime_error(field+" is out of bounds");return static_cast<int>(number);
 }
 const auto number=value.get<std::int64_t>();if(number<low||number>high)throw std::runtime_error(field+" is out of bounds");return static_cast<int>(number);
}
void validateCharts(const Json& book){
 if(!book.contains("charts"))return;
 if(!book["charts"].is_array())throw std::runtime_error("Charts must be an array");
 for(const auto& chart:book["charts"]){
  if(!chart.is_object())throw std::runtime_error("Each chart must be an object");
  for(const auto* key:{"id","type","sheet","title"})if(chart.contains(key)&&!chart[key].is_string())throw std::runtime_error(std::string("Chart ")+key+" must be text");
  const Json* pivot=nullptr;if(chart.contains("pivotId")){if(!chart.at("pivotId").is_string())throw std::runtime_error("PivotChart identity must be text");if(book.contains("pivots"))for(const auto& candidate:book.at("pivots"))if(candidate.value("version",0)==1&&candidate.at("id")==chart.at("pivotId"))pivot=&candidate;if(!pivot)throw std::runtime_error("PivotChart source identity was not found");const auto type=chart.at("type").get<std::string>();if(type!="line"&&type!="column"&&type!="bar")throw std::runtime_error("PivotCharts support line, column and bar families");const auto& output=pivot->at("output");if(chart.at("sheet")!=output.at("sheet"))throw std::runtime_error("PivotChart must stay on its report output sheet");for(const auto* key:{"row1","col1","row2","col2"})if(chart.at("range").at(key)!=output.at(key))throw std::runtime_error("PivotChart range must retain its complete report output");}
  if(chart.contains("range")){
   const auto& range=chart["range"];if(!range.is_object())throw std::runtime_error("Chart range must be an object");
   const auto r1=metadataInteger(range.at("row1"),1,maxRows,"Chart range row1"),r2=metadataInteger(range.at("row2"),1,maxRows,"Chart range row2");
   const auto c1=metadataInteger(range.at("col1"),1,maxColumns,"Chart range col1"),c2=metadataInteger(range.at("col2"),1,maxColumns,"Chart range col2");
   if(r2<r1||c2<c1||(!pivot&&(r2-r1>10000||c2-c1>100)))throw std::runtime_error("Chart range is reversed or exceeds 10000 data rows / 100 series");
  }
  if(chart.contains("anchor")){
   const auto& anchor=chart["anchor"];if(!anchor.is_object())throw std::runtime_error("Chart anchor must be an object");
   for(const auto* key:{"row","col","width","height"})if(anchor.contains(key)){
    const auto maximum=std::string(key)=="row"?maxRows:std::string(key)=="col"?maxColumns:8192;
    metadataInteger(anchor[key],1,maximum,std::string("Chart anchor ")+key);
   }
   for(const auto* key:{"offsetX","offsetY"})if(anchor.contains(key))metadataInteger(anchor[key],0,1000000,std::string("Chart anchor ")+key);
  }
 }
}
void validateVba(const Json& book){
 if(!book.contains("vba"))return;
 const auto& vba=book["vba"];if(!vba.is_object())throw std::runtime_error("VBA metadata must be an object");
 for(const auto* key:{"enabled","projectPresent","sourceAvailable"})if(vba.contains(key)&&!vba[key].is_boolean())throw std::runtime_error(std::string("VBA ")+key+" must be a boolean");
 if(vba.contains("modules")){
  if(!vba["modules"].is_array())throw std::runtime_error("VBA modules must be an array");
  for(const auto& module:vba["modules"]){
   if(!module.is_object())throw std::runtime_error("VBA modules must be objects");
   for(const auto* key:{"name","source"})if(module.contains(key)&&!module[key].is_string())throw std::runtime_error(std::string("VBA module ")+key+" must be text");
  }
 }
}
void validateCellStyle(const Json& style){
 if(!style.is_object())throw std::runtime_error("Cell style must be an object");
 for(const auto* key:{"bold","italic","underline"})if(style.contains(key)&&!style[key].is_boolean())throw std::runtime_error(std::string("Cell style ")+key+" must be a boolean");
 for(const auto* key:{"fontFamily","fill","color","alignment","numberFormat","note"})if(style.contains(key)&&!style[key].is_string())throw std::runtime_error(std::string("Cell style ")+key+" must be text");
 if(style.contains("fontSize")){
  if(!style["fontSize"].is_number())throw std::runtime_error("Cell style fontSize must be numeric");
  const auto size=style["fontSize"].get<double>();if(!std::isfinite(size)||size<1||size>400)throw std::runtime_error("Cell style fontSize must be between 1 and 400");
 }
}
void canonicalize(Json& book){
 if(!book.is_object()||!book.contains("sheets")||!book["sheets"].is_array()||book["sheets"].empty())throw std::runtime_error("Workbook must have a worksheet");
 if(!book.contains("revision"))book["revision"]=0;
 if(!book["revision"].is_number_integer()||book["revision"].get<std::int64_t>()<0)throw std::runtime_error("Invalid workbook revision");
 std::set<std::string> ids,names;
 for(auto& sheet:book["sheets"]){
  const auto id=sheet.at("id").get<std::string>(),name=sheet.at("name").get<std::string>();sheetName(name);
  if(id.empty()||!ids.insert(id).second||!names.insert(lower(name)).second)throw std::runtime_error("Duplicate worksheet identity or name");
  if(!sheet.contains("cells"))sheet["cells"]=Json::array();
  auto& cells=sheet["cells"];if(!cells.is_array())throw std::runtime_error("Cells must be an array");
  bool ordered=true,first=true;std::pair<int,int> previous;
  for(const auto& cell:cells){const auto row=cell.at("row").get<int>(),col=cell.at("col").get<int>();address(row,col);const auto key=std::pair{row,col};if(!first){if(previous==key)throw std::runtime_error("Duplicate cell address");ordered&=previous<key;}previous=key;first=false;if(!cell.at("input").is_string())throw std::runtime_error("Cell input must be text");if(cell.contains("style"))validateCellStyle(cell.at("style"));}
  if(!ordered){
   const auto key=[](const Json& cell){return std::pair{cell.at("row").get<int>(),cell.at("col").get<int>()};};
   std::sort(cells.begin(),cells.end(),[&](const auto& a,const auto& b){return key(a)<key(b);});
   for(std::size_t index=1;index<cells.size();++index)if(key(cells[index-1])==key(cells[index]))throw std::runtime_error("Duplicate cell address");
  }
  validateWorksheetLayout(book,sheet);validateWorksheetValidations(book,sheet);
 }
 validateCharts(book);
 validateVba(book);
 validatePivots(book);
 validateFinancialModels(book);
}
Json& findSheet(Json& book,const Json& op){auto& sheets=book.at("sheets");auto it=std::find_if(sheets.begin(),sheets.end(),[&](const auto& s){return s.at("id")==op.at("sheet");});if(it==sheets.end())throw std::runtime_error("Worksheet was not found");return *it;}
void validateMergedCellWrite(const Json& sheet,int row,int col,const std::string& input){
 if(input.empty()||!sheet.contains("mergedRanges"))return;
 for(const auto& range:sheet.at("mergedRanges"))if(row>=range.at("row1")&&row<=range.at("row2")&&col>=range.at("col1")&&col<=range.at("col2")&&(row!=range.at("row1")||col!=range.at("col1")))throw std::runtime_error("Cannot write inside a merged cell. Edit its upper-left cell or unmerge it first.");
}
void editWorksheetStructure(Json& book,const Json& operation){
 const auto type=operation.at("type").get<std::string>();const bool rows=type=="insert_rows"||type=="delete_rows",remove=type=="delete_rows"||type=="delete_columns";
 const int limit=rows?maxRows:maxColumns,index=metadataInteger(operation.at("index"),1,limit,"Structural edit index"),count=metadataInteger(operation.at("count"),1,limit-index+1,"Structural edit count");
 auto& target=findSheet(book,operation);const auto id=target.at("id").get<std::string>(),name=target.at("name").get<std::string>();
 for(const auto& owner:book.at("sheets"))if(owner.contains("dataValidations"))for(const auto& validation:owner.at("dataValidations")){
  const auto references=formulaReferences(validation.at("formula1").get<std::string>());if(owner.at("id")==id||std::any_of(references.begin(),references.end(),[&](const auto& ref){return lower(ref.sheet)==lower(name);}))throw std::runtime_error("Remove data validation before inserting or deleting rows or columns on its target or source worksheet");
 }
 const auto referencesSheet=[&](const auto& self,const Json& value)->bool{
  if(value.is_object()){if(value.contains("sheet")&&value.at("sheet")==id)return true;for(const auto& member:value)if(self(self,member))return true;}
  else if(value.is_array())for(const auto& member:value)if(self(self,member))return true;
  return false;
 };
 for(const auto* collection:{"charts","pivots","financialModels","scenarios","tables","links"})if(book.contains(collection)&&referencesSheet(referencesSheet,book.at(collection)))throw std::runtime_error(std::string("Move or remove dependent ")+collection+" before inserting or deleting worksheet rows or columns");
 if(book.contains("settings")&&book.at("settings").is_object()&&book.at("settings").contains("printSheets")&&book.at("settings").at("printSheets").contains(id)){const auto& print=book.at("settings").at("printSheets").at(id);if(print.contains("area")||print.contains("repeatHeaderRows"))throw std::runtime_error("Clear the saved print area and repeating headers before inserting or deleting rows or columns");}
 if(target.contains("filteredRows")&&!target.at("filteredRows").empty())throw std::runtime_error("Clear the worksheet filter before inserting or deleting rows or columns");
 const auto revision=target.value("structuralRevision",std::uint64_t{});if(revision>=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))throw std::runtime_error("Worksheet structural revision is exhausted");
 auto& cells=target["cells"];const auto coordinate=rows?"row":"col";
 if(!remove)for(const auto& cell:cells)if(cell.at(coordinate).get<int>()>=index&&cell.at(coordinate).get<int>()>limit-count)throw std::runtime_error("Insertion would move populated or formatted cells beyond the worksheet boundary");
 if(remove)cells.erase(std::remove_if(cells.begin(),cells.end(),[&](const Json& cell){const int value=cell.at(coordinate);return value>=index&&value<index+count;}),cells.end());
 for(auto& cell:cells)if(cell.at(coordinate).get<int>()>=index)cell[coordinate]=cell.at(coordinate).get<int>()+(remove?-count:count);
 for(auto& sheet:book["sheets"])for(auto& cell:sheet["cells"]){const auto input=cell.at("input").get<std::string>();if(input.starts_with('='))cell["input"]=editFormulaReferences(input,sheet.at("name").get<std::string>(),name,rows,index,count,remove);}
 if(book.contains("names"))for(auto& [key,value]:book["names"].items())if(value.is_string()){
  const auto formula=value.get<std::string>();for(const auto& reference:formulaReferences(formula))if(!reference.external&&reference.sheet.empty())throw std::runtime_error("Qualify the worksheet reference in defined name "+key+" before structural editing");
  value=editFormulaReferences(formula,{},name,rows,index,count,remove);
 }
 rebaseWorksheetLayout(target,rows,index,count,!remove);
 // Source formulas on other worksheets may have been rewritten as well.
 if(book.contains("pivots"))for(auto& pivot:book["pivots"])if(pivot.value("version",0)==1)pivot["stale"]=true;
 target["structuralRevision"]=revision+1;
}
std::uint64_t nextRevision(const Json& book){const auto rev=book.at("revision").get<std::uint64_t>();if(rev>=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))throw std::runtime_error("Workbook revision is exhausted");return rev+1;}
}
WorkbookSession::WorkbookSession():WorkbookSession(Json{{"revision",0},{"sheets",Json::array({{{"id","sheet-1"},{"name","Sheet1"},{"cells",Json::array()}}})}}){}
WorkbookSession::WorkbookSession(Json workbook):workbook_(std::move(workbook)){canonicalize(workbook_);}
Json WorkbookSession::snapshot() const{return workbook_;}
const Json& WorkbookSession::snapshotView()const noexcept{return workbook_;}
std::shared_ptr<const Json> WorkbookSession::captureSnapshot()const{return std::make_shared<const Json>(workbook_);}
const Json* WorkbookSession::cellAt(std::string_view id,int row,int col)const noexcept{for(const auto& sheet:workbook_["sheets"])if(sheet["id"]==id){const auto& cells=sheet["cells"].get_ref<const Json::array_t&>();const auto key=std::pair{row,col};const auto cell=std::lower_bound(cells.begin(),cells.end(),key,[](const Json& entry,const auto& address){return std::pair{entry["row"].get<int>(),entry["col"].get<int>()}<address;});if(cell!=cells.end()&&(*cell)["row"]==row&&(*cell)["col"]==col)return &*cell;return nullptr;}return nullptr;}
namespace {
using CellKey=std::pair<int,int>;
CellKey cellKey(const Json& cell){return {cell.at("row").get<int>(),cell.at("col").get<int>()};}
void writeCellDeltas(Json& book,const Json& changes){
 std::map<std::string,std::map<CellKey,const Json*>> sheets;
 for(const auto& change:changes)sheets[change.at("sheet").get<std::string>()][{change.at("row"),change.at("col")}]=&change.at("cell");
 for(const auto& [id,updates]:sheets){auto& sheet=findSheet(book,Json{{"sheet",id}});auto& cells=sheet["cells"].get_ref<Json::array_t&>();
  if(updates.size()==1){const auto& [key,value]=*updates.begin();const auto found=std::lower_bound(cells.begin(),cells.end(),key,[](const Json& cell,const CellKey& address){return cellKey(cell)<address;});const bool exists=found!=cells.end()&&cellKey(*found)==key;if(value->is_null()){if(exists)cells.erase(found);}else if(exists)*found=*value;else cells.insert(found,*value);continue;}
  bool resize=false;
  for(const auto& [key,value]:updates){const auto found=std::lower_bound(cells.begin(),cells.end(),key,[](const Json& cell,const CellKey& k){return cellKey(cell)<k;});if(value->is_null()||found==cells.end()||cellKey(*found)!=key){resize=true;break;}}
  if(!resize){for(const auto& [key,value]:updates){auto found=std::lower_bound(cells.begin(),cells.end(),key,[](const Json& cell,const CellKey& k){return cellKey(cell)<k;});*found=*value;}continue;}
  Json::array_t merged;merged.reserve(cells.size()+updates.size());auto change=updates.begin();
  for(auto& cell:cells){const auto key=cellKey(cell);while(change!=updates.end()&&change->first<key){if(!change->second->is_null())merged.push_back(*change->second);++change;}
   if(change!=updates.end()&&change->first==key){if(!change->second->is_null())merged.push_back(*change->second);++change;}else merged.push_back(std::move(cell));}
  while(change!=updates.end()){if(!change->second->is_null())merged.push_back(*change->second);++change;}cells.swap(merged);
 }
}
Json staleStates(const Json& book){Json states=Json::array();for(const auto* collection:{"pivots","financialModels"})if(book.contains(collection))for(const auto& report:book.at(collection))if(report.value("version",0)==1&&(std::string(collection)=="pivots"||report.contains("output")))states.push_back({{"collection",collection},{"id",report.at("id")},{"present",report.contains("stale")},{"value",report.value("stale",Json())}});return states;}
void restoreStale(Json& book,const Json& states){for(const auto& state:states){const auto collection=state.value("collection",std::string("pivots"));if(!book.contains(collection))continue;for(auto& report:book[collection])if(report.value("id",std::string())==state.at("id")){if(state.at("present").get<bool>())report["stale"]=state.at("value");else report.erase("stale");}}}
}
const Json& WorkbookSession::lastChange()const noexcept{return lastChange_;}
std::size_t WorkbookSession::historyBytes()const noexcept{return historyBytes_;}
Json WorkbookSession::takeRecoveryDelta(){auto deltas=std::move(recoveryDeltas_);recoveryDeltas_=Json::array();return deltas;}
struct WorkbookSession::PreparedTransaction::Data {
 Json candidate,recovery,publishChange,retiredBook,retiredLegacyPreview;
 std::optional<HistoryEntry> entry;
 std::vector<HistoryEntry> retiredUndo,retiredRedo;
 std::uint64_t base=0;
 bool changed=false,consumed=false,adopted=false;
};
WorkbookSession::PreparedTransaction::PreparedTransaction(std::unique_ptr<Data> data):data_(std::move(data)){}
WorkbookSession::PreparedTransaction::~PreparedTransaction()=default;
const Json& WorkbookSession::PreparedTransaction::snapshot()const noexcept{return data_->candidate;}
std::uint64_t WorkbookSession::PreparedTransaction::baseRevision()const noexcept{return data_->base;}
bool WorkbookSession::PreparedTransaction::changed()const noexcept{return data_->changed;}
std::unique_ptr<WorkbookSession::PreparedTransaction> WorkbookSession::prepareTransaction(Json captured,const Json& operations,std::string* error){
 try{
  WorkbookSession working(std::move(captured));auto data=std::make_unique<PreparedTransaction::Data>();data->base=working.workbook_.at("revision");std::string failure;
  NativePreparationProjection immutableSource(working.workbook_,operations);
  if(!working.apply(data->base,operations,&failure))throw std::runtime_error(failure);
  immutableSource.restore();
  data->changed=!working.undo_.empty();data->publishChange={{"structure",false},{"metadata",false},{"sheets",Json::array()}};
  if(data->changed){data->entry.emplace(std::move(working.undo_.back()));data->publishChange=data->entry->change;data->recovery=std::move(working.recoveryDeltas_.at(0));}
  data->retiredUndo.reserve(256);data->retiredRedo.reserve(256);data->candidate=std::move(working.workbook_);if(error)error->clear();return std::unique_ptr<PreparedTransaction>(new PreparedTransaction(std::move(data)));
 }catch(const std::exception& failure){if(error)*error=failure.what();return {};}
}
bool WorkbookSession::commitPrepared(std::uint64_t expected,PreparedTransaction& prepared,std::string* error){
 try{
  auto& data=*prepared.data_;if(data.consumed)throw std::runtime_error("Prepared transaction was already consumed");
  if(expected!=data.base||workbook_.at("revision").get<std::uint64_t>()!=expected)throw std::runtime_error("Prepared transaction revision conflict");
  if((preview_||preparedPreview_)&&preparedPreview_.get()!=&prepared)throw std::runtime_error("Accept or discard the current preview first");
  if(data.adopted&&preparedPreview_.get()!=&prepared)throw std::runtime_error("Prepared preview is no longer active");
  if(data.changed){
   constexpr std::size_t limit=512ULL*1024*1024;auto& entry=*data.entry;if(entry.bytes>limit)throw std::runtime_error("This transaction exceeds the 512 MiB undo budget; use a smaller transaction.");
   // All large JSON envelopes and deltas were built on the preparation thread.
   // Reserve bounded history slots before mutation, then publish only moves.
   undo_.reserve(256);auto& recovery=recoveryDeltas_.get_ref<Json::array_t&>();recovery.reserve(recovery.size()+1);recovery.push_back(std::move(data.recovery));
   for(const auto& redo:redo_)historyBytes_-=redo.bytes;redo_.swap(data.retiredRedo);
   while(!undo_.empty()&&(undo_.size()>=256||historyBytes_+entry.bytes>limit)){historyBytes_-=undo_.front().bytes;data.retiredUndo.push_back(std::move(undo_.front()));undo_.erase(undo_.begin());}
   historyBytes_+=entry.bytes;undo_.push_back(std::move(entry));data.retiredBook=std::move(workbook_);workbook_=std::move(data.candidate);
  }
  lastChange_.swap(data.publishChange);data.consumed=true;if(error)error->clear();return true;
 }catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
const Json* WorkbookSession::previewView()const noexcept{return preparedPreview_?&preparedPreview_->snapshot():preview_?&*preview_:nullptr;}
bool WorkbookSession::previewPrepared(std::uint64_t expected,std::unique_ptr<PreparedTransaction>& incoming,std::string* error){
 try{
  if(!incoming||incoming->data_->consumed||incoming->data_->adopted)throw std::runtime_error("A fresh prepared transaction is required");
  if(incoming->data_->base!=expected||workbook_.at("revision").get<std::uint64_t>()!=expected)throw std::runtime_error("Prepared preview revision conflict");
  if(preview_){incoming->data_->retiredLegacyPreview=std::move(*preview_);preview_.reset();}
  incoming->data_->adopted=true;preparedPreview_.swap(incoming);previewBase_=expected;if(error)error->clear();return true;
 }catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
bool WorkbookSession::acceptPrepared(std::unique_ptr<PreparedTransaction>& retired,std::string* error){
 if(retired){if(error)*error="Dispose the previous retired transaction on a worker first";return false;}
 if(!preparedPreview_){if(error)*error="No prepared preview is active";return false;}
 if(!commitPrepared(previewBase_,*preparedPreview_,error))return false;retired=std::move(preparedPreview_);return true;
}
std::unique_ptr<WorkbookSession::PreparedTransaction> WorkbookSession::discardPrepared(){if(preparedPreview_)preparedPreview_->data_->consumed=true;return std::move(preparedPreview_);}
void WorkbookSession::remember(HistoryEntry entry){
 constexpr std::size_t limit=512ULL*1024*1024;if(entry.bytes>limit)throw std::runtime_error("This transaction exceeds the 512 MiB undo budget; use a smaller transaction.");
 for(const auto& redo:redo_)historyBytes_-=redo.bytes;redo_.clear();
 while(!undo_.empty()&&(undo_.size()>=256||historyBytes_+entry.bytes>limit)){historyBytes_-=undo_.front().bytes;undo_.erase(undo_.begin());}
 recoveryDeltas_.push_back({{"version",1},{"baseRevision",workbook_.at("revision")},{"revision",nextRevision(workbook_)},{"patch",entry.patch},{"changes",entry.redo}});
 historyBytes_+=entry.bytes;undo_.push_back(std::move(entry));
}
WorkbookSession::HistoryEntry WorkbookSession::difference(const Json& before,const Json& after)const{
 HistoryEntry entry;entry.change={{"structure",false},{"metadata",false},{"sheets",Json::array()}};
 bool sameSheets=before.at("sheets").size()==after.at("sheets").size();if(sameSheets)for(std::size_t i=0;i<before["sheets"].size();++i)sameSheets&=before["sheets"][i]["id"]==after["sheets"][i]["id"];
 if(!sameSheets){entry.patch=true;entry.undo=Json::diff(after,before);entry.redo=Json::diff(before,after);entry.change["structure"]=true;entry.change["metadata"]=true;}
 else{
  entry.undo={{"cells",Json::array()},{"stale",Json::array()},{"metadata",Json::array()},{"sheetMetadata",Json::array()}};entry.redo=entry.undo;
  auto fields=[&](const Json& old,const Json& value,const std::string& target,const std::string& sheet){std::set<std::string> keys;for(const auto& [key,field]:old.items()){static_cast<void>(field);if(key!="revision"&&key!="sheets"&&key!="cells"&&key!="id")keys.insert(key);}for(const auto& [key,field]:value.items()){static_cast<void>(field);if(key!="revision"&&key!="sheets"&&key!="cells"&&key!="id")keys.insert(key);}
   for(const auto& key:keys)if(old.contains(key)!=value.contains(key)||(old.contains(key)&&old.at(key)!=value.at(key))){Json previous={{"key",key},{"present",old.contains(key)},{"value",old.value(key,Json())}},next={{"key",key},{"present",value.contains(key)},{"value",value.value(key,Json())}};if(!sheet.empty()){previous["sheet"]=sheet;next["sheet"]=sheet;}entry.undo[target].push_back(std::move(previous));entry.redo[target].push_back(std::move(next));}
  };
  fields(before,after,"metadata","");
  for(std::size_t index=0;index<before["sheets"].size();++index){const auto& oldSheet=before["sheets"][index];const auto& newSheet=after["sheets"][index];const auto id=oldSheet["id"].get<std::string>();fields(oldSheet,newSheet,"sheetMetadata",id);
   const auto& oldCells=oldSheet["cells"];const auto& newCells=newSheet["cells"];std::size_t a=0,b=0;int r1=maxRows,c1=maxColumns,r2=0,c2=0;
   while(a<oldCells.size()||b<newCells.size()){const auto oldKey=a<oldCells.size()?cellKey(oldCells[a]):CellKey{maxRows+1,maxColumns+1},newKey=b<newCells.size()?cellKey(newCells[b]):CellKey{maxRows+1,maxColumns+1};const auto key=std::min(oldKey,newKey);const Json* previous=oldKey==key?&oldCells[a]:nullptr;const Json* next=newKey==key?&newCells[b]:nullptr;
    if(!previous||!next||*previous!=*next){entry.undo["cells"].push_back({{"sheet",id},{"row",key.first},{"col",key.second},{"cell",previous?*previous:Json()}});entry.redo["cells"].push_back({{"sheet",id},{"row",key.first},{"col",key.second},{"cell",next?*next:Json()}});r1=std::min(r1,key.first);r2=std::max(r2,key.first);c1=std::min(c1,key.second);c2=std::max(c2,key.second);}
    if(previous)++a;if(next)++b;
   }
   if(r2)entry.change["sheets"].push_back({{"sheet",id},{"row1",r1},{"col1",c1},{"row2",r2},{"col2",c2}});
  }
  entry.change["metadata"]=!entry.redo["metadata"].empty();entry.change["structure"]=!entry.redo["sheetMetadata"].empty();
 }
 entry.bytes=3*(entry.undo.dump().size()+entry.redo.dump().size());return entry;
}
bool WorkbookSession::applyCells(const Json& operations,std::string* error){
 struct Pending {Json before,after;};std::map<std::string,std::map<CellKey,Pending>> staged;
 auto stage=[&](const std::string& id,int row,int col)->Pending&{address(row,col);auto& cells=staged[id];auto found=cells.find({row,col});if(found!=cells.end())return found->second;const auto* old=cellAt(id,row,col);Json before=old?*old:Json();return cells.emplace(CellKey{row,col},Pending{before,old?*old:Json{{"row",row},{"col",col},{"input",""}}}).first->second;};
 for(const auto& op:operations){const auto type=op.at("type").get<std::string>(),id=op.at("sheet").get<std::string>();const auto& sheet=findSheet(workbook_,op);validatePivotEdit(workbook_,op);validateFinancialModelEdit(workbook_,op);
  if(type=="clear_range"){const int r1=op.at("row1"),r2=op.at("row2"),c1=op.at("col1"),c2=op.at("col2");address(r1,c1);address(r2,c2);if(r2<r1||c2<c1)throw std::runtime_error("Range is reversed");for(const auto& cell:sheet.at("cells")){const auto [r,c]=cellKey(cell);if(r>=r1&&r<=r2&&c>=c1&&c<=c2)stage(id,r,c);}
   for(auto& [key,entry]:staged[id])if(key.first>=r1&&key.first<=r2&&key.second>=c1&&key.second<=c2){entry.after["input"]="";if(!entry.after.contains("style"))entry.after=nullptr;}
  }else{const int row=op.at("row"),col=op.at("col");auto& cell=stage(id,row,col).after;if(cell.is_null())cell={{"row",row},{"col",col},{"input",""}};
   if(type=="set_cell"){if(!op.at("input").is_string())throw std::runtime_error("Cell input must be text");validateMergedCellWrite(sheet,row,col,op.at("input").get_ref<const std::string&>());cell["input"]=op.at("input");}
   if(op.contains("style")){validateCellStyle(op.at("style"));if(op.value("replace",false)||op.value("replaceStyle",false)||!cell.contains("style"))cell["style"]=Json::object();cell["style"].update(op.at("style"));}
  }
 }
 HistoryEntry entry;entry.undo={{"cells",Json::array()},{"stale",staleStates(workbook_)}};entry.redo={{"cells",Json::array()}};entry.change={{"structure",false},{"metadata",false},{"sheets",Json::array()}};
 for(const auto& [id,cells]:staged){int r1=maxRows,c1=maxColumns,r2=0,c2=0;for(const auto& [key,cell]:cells)if(cell.before!=cell.after){entry.undo["cells"].push_back({{"sheet",id},{"row",key.first},{"col",key.second},{"cell",cell.before}});entry.redo["cells"].push_back({{"sheet",id},{"row",key.first},{"col",key.second},{"cell",cell.after}});r1=std::min(r1,key.first);r2=std::max(r2,key.first);c1=std::min(c1,key.second);c2=std::max(c2,key.second);}if(r2)entry.change["sheets"].push_back({{"sheet",id},{"row1",r1},{"col1",c1},{"row2",r2},{"col2",c2}});}
 if(entry.redo["cells"].empty()){lastChange_={{"structure",false},{"metadata",false},{"sheets",Json::array()}};if(error)error->clear();return true;}
 const auto revision=nextRevision(workbook_);entry.bytes=3*(entry.undo.dump().size()+entry.redo.dump().size())+4096;if(entry.bytes>512ULL*1024*1024)throw std::runtime_error("This transaction exceeds the 512 MiB undo budget; use a smaller transaction.");
 writeCellDeltas(workbook_,entry.redo["cells"]);for(const auto& op:operations){invalidatePivots(workbook_,op);invalidateFinancialModels(workbook_,op);}entry.redo["stale"]=staleStates(workbook_);entry.change["metadata"]=entry.undo["stale"]!=entry.redo["stale"];lastChange_=entry.change;remember(std::move(entry));workbook_["revision"]=revision;if(error)error->clear();return true;
}
void WorkbookSession::restore(const HistoryEntry& entry,bool forward){
 const auto revision=nextRevision(workbook_);const auto& changes=forward?entry.redo:entry.undo;
 if(entry.patch){NativePreparationProjection immutableSource(workbook_,changes,NativePreparationProjection::HistoryPatch{});workbook_=workbook_.patch(changes);immutableSource.restore(true);}else{writeCellDeltas(workbook_,changes.at("cells"));restoreStale(workbook_,changes.at("stale"));
  for(const auto* field:{"metadata","sheetMetadata"})if(changes.contains(field))for(const auto& change:changes.at(field)){auto& object=std::string(field)=="metadata"?workbook_:findSheet(workbook_,change);const auto key=change.at("key").get<std::string>();if(change.at("present").get<bool>())object[key]=change.at("value");else object.erase(key);}
 }
 recoveryDeltas_.push_back({{"version",1},{"baseRevision",revision-1},{"revision",revision},{"patch",entry.patch},{"changes",changes}});
 workbook_["revision"]=revision;lastChange_=entry.change;
}
bool WorkbookSession::apply(std::uint64_t expected,const Json& operations,std::string* error){
 try{
  if(workbook_.at("revision").get<std::uint64_t>()!=expected)throw std::runtime_error("Workbook revision conflict");
  if(!operations.is_array())throw std::runtime_error("Operations must be an array");
  const bool cellsOnly=std::all_of(operations.begin(),operations.end(),[](const Json& op){const auto type=op.value("type",std::string());return type=="set_cell"||type=="set_style"||type=="clear_range";});
  if(cellsOnly)return applyCells(operations,error);
  auto candidate=workbook_;
  std::map<std::string,std::map<std::pair<int,int>,std::size_t>> indexes;
  for(const auto& op:operations){
   const auto type=op.at("type").get<std::string>();
   if(type=="replace_pivot"||type=="delete_pivot"){validateFinancialModelEdit(candidate,op);applyPivotTransaction(candidate,op);invalidateFinancialModels(candidate,op);indexes.clear();continue;}
   if(type=="save_financial_definition"||type=="apply_financial_scenario"||type=="replace_financial_model"||type=="delete_financial_model"){
    applyFinancialModelTransaction(candidate,op);indexes.clear();
    if(type=="apply_financial_scenario")for(const auto& definition:candidate.at("financialModels"))if(definition.at("id")==op.at("id"))for(const auto& cell:definition.at("cells"))invalidatePivots(candidate,{{"type","set_cell"},{"sheet",cell.at("sheet")},{"row",cell.at("row")},{"col",cell.at("col")}});
    invalidateFinancialModels(candidate,op);continue;
   }
   validatePivotEdit(candidate,op);validateFinancialModelEdit(candidate,op);
   if(type=="set_data_validation"){
    auto& sheet=findSheet(candidate,op);auto rules=sheet.value("dataValidations",Json::array());const auto& validation=op.at("validation");const auto range=validation.is_null()?op.at("range"):validation.at("range");
    rules.erase(std::remove_if(rules.begin(),rules.end(),[&](const Json& rule){return rule.at("range")==range;}),rules.end());
    if(!validation.is_null())rules.push_back(validation);if(rules.empty())sheet.erase("dataValidations");else sheet["dataValidations"]=std::move(rules);
   }
   else if(type=="set_sheet_layout")applyWorksheetLayout(candidate,op);
   else if(type=="insert_rows"||type=="delete_rows"||type=="insert_columns"||type=="delete_columns"){editWorksheetStructure(candidate,op);indexes.clear();}
   else if(type=="add_sheet")candidate["sheets"].push_back({{"id",op.at("id")},{"name",op.at("name")},{"cells",Json::array()}});
   else if(type=="rename_sheet"){
    auto& sheet=findSheet(candidate,op);const auto oldName=sheet.at("name").get<std::string>(),newName=op.at("name").get<std::string>();sheet["name"]=newName;
    for(auto& entry:candidate["sheets"])for(auto& cell:entry["cells"]){auto input=cell.at("input").get<std::string>();if(input.starts_with('='))cell["input"]=renameSheetReferences(input,oldName,newName);}
    for(auto& entry:candidate["sheets"])if(entry.contains("dataValidations"))for(auto& rule:entry["dataValidations"])rule["formula1"]=renameSheetReferences(rule.at("formula1").get<std::string>(),oldName,newName);
    if(candidate.contains("names"))for(auto& value:candidate["names"])if(value.is_string())value=renameSheetReferences(value.get<std::string>(),oldName,newName);
   }
   else if(type=="set_cell"||type=="set_style"){
    auto& sheet=findSheet(candidate,op);const auto id=sheet.at("id").get<std::string>();auto found=indexes.find(id);if(found==indexes.end()){std::map<std::pair<int,int>,std::size_t> index;for(std::size_t i=0;i<sheet["cells"].size();++i)index[{sheet["cells"][i]["row"],sheet["cells"][i]["col"]}]=i;found=indexes.emplace(id,std::move(index)).first;}
    const int row=op.at("row"),col=op.at("col");address(row,col);auto& index=found->second;const auto key=std::pair{row,col};if(!index.contains(key)){index[key]=sheet["cells"].size();sheet["cells"].push_back({{"row",row},{"col",col},{"input",""}});}auto& cell=sheet["cells"][index.at(key)];
    if(type=="set_cell"){if(!op.at("input").is_string())throw std::runtime_error("Cell input must be text");validateMergedCellWrite(sheet,row,col,op.at("input").get_ref<const std::string&>());cell["input"]=op.at("input");}
    if(op.contains("style")){if(!op["style"].is_object())throw std::runtime_error("Cell style must be an object");if(op.value("replace",false)||op.value("replaceStyle",false)||!cell.contains("style"))cell["style"]=Json::object();cell["style"].update(op["style"]);}
   }else if(type=="clear_range"){
    indexes.clear();
    auto& cells=findSheet(candidate,op)["cells"];
    const auto r1=op.at("row1").get<int>(),c1=op.at("col1").get<int>(),r2=op.at("row2").get<int>(),c2=op.at("col2").get<int>();address(r1,c1);address(r2,c2);if(r2<r1||c2<c1)throw std::runtime_error("Range is reversed");
    for(auto& cell:cells)if(cell["row"]>=r1&&cell["row"]<=r2&&cell["col"]>=c1&&cell["col"]<=c2)cell["input"]="";
    cells.erase(std::remove_if(cells.begin(),cells.end(),[](const auto& cell){return cell.at("input")==""&&!cell.contains("style");}),cells.end());
   }else if(type=="replace_sheet_cells"){
    indexes.clear();
    if(!op.at("cells").is_array())throw std::runtime_error("Cells must be an array");
    findSheet(candidate,op)["cells"]=op.at("cells");
   }else if(type=="delete_sheet"){
    indexes.clear();
    auto& sheets=candidate["sheets"];if(sheets.size()==1)throw std::runtime_error("Cannot delete the last worksheet");findSheet(candidate,op);sheets.erase(std::remove_if(sheets.begin(),sheets.end(),[&](const auto& sheet){return sheet.at("id")==op.at("sheet");}),sheets.end());
   }else if(type=="set_metadata"){
    const auto key=op.at("key").get<std::string>();
    const std::set<std::string> allowed{"charts","pivots","scenarios","links","settings","vba","names"};
    if(!allowed.contains(key))throw std::runtime_error("Unsupported workbook metadata: "+key);
    candidate[key]=op.at("value");
   }else throw std::runtime_error("Unsupported workbook operation: "+type);
   invalidatePivots(candidate,op);invalidateFinancialModels(candidate,op);
  }
  canonicalize(candidate);if(candidate==workbook_){if(error)error->clear();return true;}
  auto entry=difference(workbook_,candidate);
  candidate["revision"]=nextRevision(workbook_);lastChange_=entry.change;remember(std::move(entry));workbook_=std::move(candidate);if(error)error->clear();return true;
 }catch(const std::exception& ex){if(error)*error=ex.what();return false;}
}
bool WorkbookSession::canUndo() const{return !undo_.empty();}
bool WorkbookSession::canRedo() const{return !redo_.empty();}
bool WorkbookSession::hasPreview() const{return preview_.has_value()||static_cast<bool>(preparedPreview_);}
bool WorkbookSession::undo(std::string* error){
 if(undo_.empty()){if(error)*error="Nothing to undo";return false;}
 try{restore(undo_.back(),false);redo_.push_back(std::move(undo_.back()));undo_.pop_back();discard();if(error)error->clear();return true;}catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
bool WorkbookSession::redo(std::string* error){
 if(redo_.empty()){if(error)*error="Nothing to redo";return false;}
 try{restore(redo_.back(),true);undo_.push_back(std::move(redo_.back()));redo_.pop_back();discard();if(error)error->clear();return true;}catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
Json WorkbookSession::preview(std::uint64_t expected,const Json& operations,std::string* error){
 WorkbookSession candidate(workbook_);if(!candidate.apply(expected,operations,error))return nullptr;preparedPreview_.reset();preview_=candidate.snapshot();previewBase_=expected;return *preview_;
}
bool WorkbookSession::accept(std::string* error){
 if(preparedPreview_){std::unique_ptr<PreparedTransaction> retired;return acceptPrepared(retired,error);}
 if(!preview_){if(error)*error="No preview is active";return false;}
 if(workbook_.at("revision").get<std::uint64_t>()!=previewBase_){if(error)*error="Preview revision conflict";return false;}
 try{if(*preview_!=workbook_){auto candidate=*preview_;candidate["revision"]=workbook_["revision"];auto entry=difference(workbook_,candidate);lastChange_=entry.change;remember(std::move(entry));workbook_=std::move(*preview_);}discard();if(error)error->clear();return true;}catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
void WorkbookSession::discard(){preview_.reset();preparedPreview_.reset();}
}
