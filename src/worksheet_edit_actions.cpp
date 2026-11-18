#include "tigersheets/worksheet_edit_actions.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QItemSelectionModel>
#include <QTableView>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QAbstractItemDelegate>
#include <QStyleOptionViewItem>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>
namespace tigersheets {
namespace {
int normalDigitWidth(const QTableView&){return SpreadsheetNormalDigitWidth;}
QItemSelection selectedRanges(QTableView& view){
 auto ranges=view.selectionModel()->selection();if(ranges.isEmpty()){if(!view.currentIndex().isValid())throw std::runtime_error("Select a worksheet cell first.");ranges.select(view.currentIndex(),view.currentIndex());}return ranges;
}
QItemSelectionRange selectedRange(QTableView& view){
 const auto ranges=selectedRanges(view);if(ranges.size()>1)throw std::runtime_error("Select one contiguous rectangular range.");return ranges.front();
}
using AxisIntervals=std::vector<std::pair<int,int>>;
AxisIntervals selectedAxes(const QItemSelection& ranges,bool rows){
 AxisIntervals intervals;for(const auto& range:ranges)intervals.emplace_back((rows?range.top():range.left())+1,(rows?range.bottom():range.right())+1);std::sort(intervals.begin(),intervals.end());AxisIntervals result;
 for(const auto& interval:intervals){if(result.empty()||interval.first>result.back().second+1)result.push_back(interval);else result.back().second=std::max(result.back().second,interval.second);}return result;
}
bool applyOperations(WorksheetModel& model,const Json& operations){if(!model.apply(operations))throw std::runtime_error(model.lastError().toStdString());return true;}
bool overlaps(const Json& a,const Json& b){return a.at("row1")<=b.at("row2")&&b.at("row1")<=a.at("row2")&&a.at("col1")<=b.at("col2")&&b.at("col1")<=a.at("col2");}
bool contains(const Json& outer,const Json& inner){return outer.at("row1")<=inner.at("row1")&&outer.at("col1")<=inner.at("col1")&&outer.at("row2")>=inner.at("row2")&&outer.at("col2")>=inner.at("col2");}
}
bool executeWorksheetEdit(QWidget* parent,WorksheetModel& model,QTableView& view,const QString& command,const Json& arguments,const WorkbookPackage* package){
 if(!(model.flags(view.currentIndex())&Qt::ItemIsEditable))throw std::runtime_error("Accept or discard the preview before editing the worksheet.");
 if(command=="cells.format")return formatWorksheetCells(parent,model,view,arguments);
 if(command=="cells.clearFormats")return clearWorksheetFormats(model,view);
 if(command=="columns.autofit"){
  auto ranges=selectedRanges(view);if(ranges.size()==1&&ranges.front().width()==1&&ranges.front().height()==1){const int column=ranges.front().left();ranges=QItemSelection(model.index(0,column),model.index(model.rowCount()-1,column));}const auto axes=selectedAxes(ranges,false);const auto selected=[&](int column){return std::any_of(axes.begin(),axes.end(),[&](const auto& interval){return column>=interval.first&&column<=interval.second;});};
  for(const auto& sheet:model.workbookView().at("sheets"))if(sheet.at("id")==model.sheetId().toStdString()){
   std::map<int,Json> columns;for(const auto& item:sheet.value("columnLayout",Json::array()))columns[item.at("index")]=item;for(const auto& index:sheet.value("hiddenColumns",Json::array())){const int column=index.get<int>();if(!columns.contains(column))columns[column]={{"index",column}};columns[column]["hidden"]=true;}
   const auto original=columns;std::map<int,int> widths;if(const auto* worksheetView=dynamic_cast<const WorksheetView*>(&view))widths=worksheetView->autoFitColumnPixels(ranges);else{QStyleOptionViewItem option;option.initFrom(&view);for(const auto& cell:sheet.at("cells")){const auto index=model.index(cell.at("row").get<int>()-1,cell.at("col").get<int>()-1);if(cell.at("input")==""||!ranges.contains(index))continue;const int width=view.itemDelegateForIndex(index)->sizeHint(option,index).width();if(width>0)widths[index.column()]=std::max(widths[index.column()],width);}}
   // Blank selected columns return to the default without materializing an
   // override for every column of a whole-row or whole-sheet selection.
   for(auto it=columns.begin();it!=columns.end();){if(selected(it->first)&&!widths.contains(it->first-1)){it->second.erase("size");it->second.erase("bestFit");if(it->second.size()==1){it=columns.erase(it);continue;}}++it;}
   const int digitWidth=normalDigitWidth(view);for(const auto& [column,pixels]:widths){if(!selected(column+1))continue;auto& entry=columns[column+1];if(entry.is_null())entry={{"index",column+1}};entry["size"]=std::clamp(std::floor(pixels*256.0/digitWidth)/256.0,1.0/256.0,255.0);entry["bestFit"]=true;}
   if(columns==original)return true;
   Json layout=Json::array();for(const auto& [index,item]:columns)layout.push_back(item);return applyOperations(model,Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"columns",std::move(layout)}}}));
  }
  return false;
 }
 if(command=="cells.merge"||command=="cells.mergeCenter"||command=="cells.unmerge"){
  const auto selection=selectedRange(view);const bool unmerge=command=="cells.unmerge";
  if(!unmerge&&(static_cast<std::int64_t>(selection.height())*selection.width()>100000||selection.height()*selection.width()==1))throw std::runtime_error("Select between two and 100,000 cells to merge.");
  const Json rectangle={{"row1",selection.top()+1},{"col1",selection.left()+1},{"row2",selection.bottom()+1},{"col2",selection.right()+1}};
  const Json* sheet=nullptr;for(const auto& entry:model.workbookView().at("sheets"))if(entry.at("id")==model.sheetId().toStdString())sheet=&entry;if(!sheet)throw std::runtime_error("The worksheet is no longer available.");
  Json merged=Json::array();for(const auto& range:sheet->value("mergedRanges",Json::array())){if(!overlaps(rectangle,range)){merged.push_back(range);continue;}if(!unmerge&&!contains(rectangle,range))throw std::runtime_error("Select the whole existing merged cell before merging this range.");}
  if(!unmerge)merged.push_back(rectangle);if(package)preflightWorksheetLayout(*package,model.sheetId().toStdString(),merged);
  Json operations=Json::array();if(!unmerge){
   for(const auto& cell:sheet->at("cells"))if(cell.at("row")>=rectangle.at("row1")&&cell.at("row")<=rectangle.at("row2")&&cell.at("col")>=rectangle.at("col1")&&cell.at("col")<=rectangle.at("col2")&&(cell.at("row")!=rectangle.at("row1")||cell.at("col")!=rectangle.at("col1"))&&cell.at("input")!="")operations.push_back({{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",cell.at("row")},{"col",cell.at("col")},{"input",""}});
   if(!operations.empty()){
    if(arguments.contains("discardOtherValues")){if(!arguments.at("discardOtherValues").get<bool>())return false;}
    else if(QMessageBox::warning(parent,"Merge cells","Merging keeps only the upper-left value. Other values in the selection will be removed. Continue?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return false;
   }
   if(command=="cells.mergeCenter")operations.push_back({{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",rectangle.at("row1")},{"col",rectangle.at("col1")},{"style",{{"alignment","center"}}}});
  }
  operations.push_back({{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"mergedRanges",std::move(merged)}});return applyOperations(model,operations);
 }
 const bool rows=command.startsWith("rows.");
 if(command=="rows.size"||command=="columns.size"||command=="rows.hide"||command=="rows.unhide"||command=="columns.hide"||command=="columns.unhide"){
  const auto axes=selectedAxes(selectedRanges(view),rows);const int first=axes.front().first;std::int64_t count=0;for(const auto& [begin,end]:axes)count+=end-begin+1;
  if(rows&&count>10000)throw std::runtime_error("Select at most 10,000 rows for a layout change.");
  const Json* sheet=nullptr;for(const auto& entry:model.workbookView().at("sheets"))if(entry.at("id")==model.sheetId().toStdString())sheet=&entry;if(!sheet)throw std::runtime_error("The worksheet is no longer available.");
  std::map<int,Json> layout;for(const auto& entry:sheet->value(rows?"rowLayout":"columnLayout",Json::array()))layout[entry.at("index").get<int>()]=entry;
  for(const auto& index:sheet->value(rows?"hiddenRows":"hiddenColumns",Json::array())){const int at=index.get<int>();if(!layout.contains(at))layout[at]={{"index",at}};layout[at]["hidden"]=true;}const auto original=layout;
  const int digitWidth=normalDigitWidth(view);double size=0;const bool resize=command.endsWith("size");if(resize){
   if(arguments.contains("size"))size=arguments.at("size").get<double>();
   else{bool accepted=false;const double fallback=rows?view.verticalHeader()->defaultSectionSize()*72.0/view.logicalDpiY():std::max(0.0,(view.horizontalHeader()->defaultSectionSize()-5.0)/digitWidth);const auto found=layout.find(first);double current=fallback;if(found!=layout.end()&&found->second.contains("size"))current=rows?found->second.at("size").get<double>():spreadsheetColumnCharacters(found->second.at("size").get<double>(),digitWidth);size=QInputDialog::getDouble(parent,rows?"Row Height":"Column Width",rows?"Height (points)":"Width (characters)",current,0,rows?409.5:spreadsheetColumnCharacters(255.0,digitWidth),2,&accepted);
    // A modal opened from a popup can leave its worksheet window inactive.
    view.window()->activateWindow();view.setFocus(Qt::ShortcutFocusReason);
    if(!accepted)return false;
   }
   if(!std::isfinite(size)||size<0||size>(rows?409.5:255.0))throw std::runtime_error("The dimension is outside Excel's row height or column width limits.");
  }
  for(const auto& [begin,end]:axes)for(int at=begin;at<=end;++at){
   if(!resize&&command.endsWith("unhide")&&!layout.contains(at))continue;
   auto& entry=layout[at];if(entry.is_null())entry={{"index",at}};
   if(resize&&!rows)entry.erase("bestFit");
   if((resize&&size==0)||command.endsWith(".hide"))entry["hidden"]=true;
   else{entry.erase("hidden");if(resize)entry["size"]=rows?size:spreadsheetColumnWidth(size,digitWidth);else if(!entry.contains("size"))layout.erase(at);}
  }
  if(layout==original)return true;Json entries=Json::array();for(const auto& [index,entry]:layout)entries.push_back(entry);
  Json operation={{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{rows?"rows":"columns",std::move(entries)}};return applyOperations(model,Json::array({std::move(operation)}));
 }
 if(command=="rows.insertAbove"||command=="rows.insertBelow"||command=="columns.insertLeft"||command=="columns.insertRight"||command=="rows.delete"||command=="columns.delete"){
  const auto range=selectedRange(view);const bool remove=command.endsWith("delete"),after=command.endsWith("Below")||command.endsWith("Right");
  const int count=rows?range.height():range.width();const int index=(rows?(after?range.bottom()+1:range.top()):(after?range.right()+1:range.left()))+1;
  if(package)preflightWorksheetStructure(*package,model.sheetId().toStdString());
  return applyOperations(model,Json::array({{{"type",std::string(remove?"delete_":"insert_")+(rows?"rows":"columns")},{"sheet",model.sheetId().toStdString()},{"index",index},{"count",count}}}));
 }
 return false;
}
}
