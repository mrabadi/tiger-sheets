#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QFontMetricsF>
#include <QTemporaryDir>
#include <QtMath>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

int renderedWidth(const WorksheetModel& model,const QModelIndex& index){
 const QFontMetricsF metrics(model.data(index,Qt::FontRole).value<QFont>());
 qreal width=0;
 for(const auto& line:model.data(index,Qt::DisplayRole).toString().split('\n'))width=std::max(width,metrics.horizontalAdvance(line));
 return qCeil(width+model.cellStyle(index.row(),index.column()).value("indent",0)*metrics.horizontalAdvance("   "))+(model.numericValue(index.row(),index.column())?6:13);
}

void renderedFontFit(){
 MainWindow window;window.resize(1280,720);window.show();QApplication::processEvents();
 auto* view=window.findChild<WorksheetView*>("worksheet.grid");
 auto* model=dynamic_cast<WorksheetModel*>(view->model());const auto id=model->sheetId().toStdString();
 require(model->apply(Json::array({
  {{"type","set_sheet_layout"},{"sheet",id},{"defaultStyle",{{"fontFamily","Calibri"},{"fontSize",12}}}},
  {{"type","set_cell"},{"sheet",id},{"row",2},{"col",2},{"input","Income Statement:"},{"style",{{"bold",true}}}},
  {{"type","set_cell"},{"sheet",id},{"row",2},{"col",3},{"input","=DATE(2019,12,31)"},{"style",{{"numberFormat","mm/dd/yyyy"}}}}
 })),"native formatted cells");
 window.recalculateNow();
 const auto dateIndex=model->index(1,2),titleIndex=model->index(1,1);
 require(model->data(dateIndex,Qt::DisplayRole).toString()=="12/31/2019","date formula has a current formatted result before measurement");
 const auto originalCells=model->workbookView().at("sheets")[0].at("cells");
 for(const auto index:{titleIndex,dateIndex}){
  view->setCurrentIndex(index);require(window.executeCommand("columns.autofit"),"actual native AutoFit command");
  std::cout<<"column="<<index.column()+1<<" fitted="<<view->columnWidth(index.column())<<" rendered="<<renderedWidth(*model,index)<<" font="<<model->data(index,Qt::FontRole).value<QFont>().family().toStdString()<<'\n';
 }
 const bool dateFits=view->columnWidth(2)>=renderedWidth(*model,dateIndex);
 const bool titleFits=view->columnWidth(1)>=renderedWidth(*model,titleIndex);
 require(model->workbookView().at("sheets")[0].at("cells")==originalCells,"AutoFit preserves formulas and authored cell styles");
 require(model->apply(Json::array({{{"type","set_cell"},{"sheet",id},{"row",5000},{"col",2},{"input","Net PP&E, Goodwill & Intangibles:"},{"style",{{"indent",1}}}}})),"long indented label beyond the initial viewport");
 const auto longIndex=model->index(4999,1);const auto displayFont=model->data(longIndex,Qt::FontRole).value<QFont>();
 const auto cellsWithLongLabel=model->workbookView().at("sheets")[0].at("cells");
 view->selectColumn(1);require(window.executeCommand("columns.autofit"),"fit the whole selected column including offscreen rows");
 const bool longFits=view->columnWidth(1)>=renderedWidth(*model,longIndex);
 std::cout<<"long fitted="<<view->columnWidth(1)<<" rendered="<<renderedWidth(*model,longIndex)<<std::endl;
 require(dateFits&&titleFits&&longFits,"AutoFit must fit the actual rendered date, bold title, and indented long text rather than an unavailable authored font");
 require(model->workbookView().at("sheets")[0].at("cells")==cellsWithLongLabel,"AutoFit only changes column geometry");
 require(model->data(longIndex,Qt::FontRole).value<QFont>()==displayFont&&model->cellStyle(longIndex.row(),longIndex.column()).value("fontFamily",std::string("Calibri"))=="Calibri","AutoFit preserves both the rendered fallback and authored font declaration");
 for(const auto& column:model->workbookView().at("sheets")[0].at("columnLayout"))require(column.value("bestFit",false),"AutoFit records native bestFit");
 require(window.executeCommand("columns.size",{{"size",20.0}}),"manual column width after AutoFit");
 for(const auto& column:model->workbookView().at("sheets")[0].at("columnLayout"))if(column.at("index")==2)require(!column.value("bestFit",false),"manual width clears bestFit");
 require(window.executeCommand("undo"),"manual width is one Undo");
 for(const auto& column:model->workbookView().at("sheets")[0].at("columnLayout"))if(column.at("index")==2)require(column.value("bestFit",false),"Undo restores bestFit");
}

void selectedRangeMeasurement(){
 MainWindow window;auto* view=window.findChild<WorksheetView*>("worksheet.grid");
 auto* model=dynamic_cast<WorksheetModel*>(view->model());const auto id=model->sheetId().toStdString();
 require(model->apply(Json::array({
  {{"type","set_cell"},{"sheet",id},{"row",1},{"col",2},{"input",std::string(100,'W')}},
  {{"type","set_cell"},{"sheet",id},{"row",2},{"col",2},{"input","Short"}},
  {{"type","set_cell"},{"sheet",id},{"row",3},{"col",2},{"input","Selected longest line\nx"},{"style",{{"indent",1},{"bold",true}}}},
  {{"type","set_cell"},{"sheet",id},{"row",4},{"col",2},{"input",std::string(90,'W')}},
  {{"type","set_cell"},{"sheet",id},{"row",6},{"col",2},{"input","Another label"}},
  {{"type","set_cell"},{"sheet",id},{"row",2},{"col",4},{"input","=DATE(2019,12,31)"},{"style",{{"numberFormat","mm/dd/yyyy"}}}},
  {{"type","set_cell"},{"sheet",id},{"row",20},{"col",6},{"input","=SEQUENCE(2,2,12345678)"}},
  {{"type","set_cell"},{"sheet",id},{"row",2},{"col",8},{"input",std::string(100,'M')}},
  {{"type","set_cell"},{"sheet",id},{"row",1},{"col",12},{"input","=\"\""}},
  {{"type","set_sheet_layout"},{"sheet",id},{"mergedRanges",Json::array({{{"row1",2},{"row2",2},{"col1",8},{"col2",9}}})}}
 })),"selected-range measurement fixture");
 window.recalculateNow();require(model->raw(20,6).isEmpty()&&model->numericValue(20,6)==12345681,"real calculated spill destination has a value without a stored input");
 QItemSelection selected(model->index(1,1),model->index(2,3));
 selected.select(model->index(5,1),model->index(5,1));
 selected.select(model->index(1,1),model->index(1,1));
 selected.select(model->index(20,6),model->index(20,6));
 selected.select(model->index(1,7),model->index(1,8));
 selected.select(model->index(0,10),model->index(0,11));
 const auto bookBefore=model->workbookView(),cacheBefore=model->calculatedSnapshot();
 const auto selectionBefore=view->selectionModel()->selection();const auto currentBefore=view->currentIndex();
 const auto widths=view->autoFitColumnPixels(selected);
 require(widths.size()==3,"only selected nonblank unmerged columns are measured; blank and empty-formula columns remain caller defaults");
 const int selectedLabelWidth=std::max({renderedWidth(*model,model->index(1,1)),renderedWidth(*model,model->index(2,1)),renderedWidth(*model,model->index(5,1))});
 require(widths.at(1)==selectedLabelWidth,"disjoint and overlapping row intervals measure their union without including outside or gap outliers");
 require(widths.at(3)==renderedWidth(*model,model->index(1,3)),"selected formula measures its formatted current date");
 require(widths.at(6)==renderedWidth(*model,model->index(20,6)),"selected current spill destination participates without a stored cell");
 require(view->autoFitColumnPixels(1)>widths.at(1),"scalar compatibility still measures the entire column");
 require(view->autoFitColumnPixels(6)==widths.at(6),"whole-column compatibility also includes current spill destinations");
 require(view->autoFitColumnPixels(QItemSelection()).empty(),"empty selection has no measured columns");
 require(model->workbookView()==bookBefore&&model->calculatedSnapshot()==cacheBefore,"measurement preserves workbook inputs, styles, calculation values, and revision");
 require(view->selectionModel()->selection()==selectionBefore&&view->currentIndex()==currentBefore,"measurement never mutates the current cell or selection");
 require(model->apply(Json::array({{{"type","set_cell"},{"sheet",id},{"row",20},{"col",6},{"input","9"}}})),"replace the spill origin without publishing another calculation");
 require(model->calculatedSnapshot().at("status")=="stale","input edits invalidate the previous spill cache");
 require(view->autoFitColumnPixels(QItemSelection(model->index(20,6),model->index(20,6))).empty(),"stale spill results must never inflate a now-blank destination column");
}

int main(int argc,char** argv){
 QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());
 try{renderedFontFit();selectedRangeMeasurement();std::cout<<"Source AutoFit measurement passed\n";}
 catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
