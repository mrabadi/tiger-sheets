#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QScrollBar>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void incrementalDimensions(double defaultRowHeight=15.0){
 auto book=WorkbookSession().snapshot();auto& sheet=book["sheets"][0];sheet["defaultRowHeight"]=defaultRowHeight;sheet["defaultColumnWidth"]=12.0;sheet["mergedRanges"]=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",2}}});sheet["rowLayout"]=Json::array();sheet["columnLayout"]=Json::array();
 for(int i=1;i<=8;++i){sheet["rowLayout"].push_back({{"index",i},{"size",30.0},{"hidden",i==8}});sheet["columnLayout"].push_back({{"index",i},{"size",14.0},{"hidden",i==8}});}sheet["cells"]=Json::array({{{"row",3},{"col",1},{"input","keep"}},{{"row",4},{"col",2},{"input","=1+2"}}});
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.verticalHeader()->setDefaultSectionSize(20);view.horizontalHeader()->setDefaultSectionSize(64);view.setModel(&model);view.resize(800,400);view.show();view.setCurrentIndex(model.index(2,0));QApplication::processEvents();const auto before=session.snapshot();const auto selected=view.selectionModel()->selection();const auto current=view.currentIndex();
 int horizontal=0,vertical=0,verticalSettled=0;QObject observer;QObject::connect(view.horizontalHeader(),&QHeaderView::sectionResized,&observer,[&]{++horizontal;});QObject::connect(view.verticalHeader(),&QHeaderView::sectionResized,&observer,[&]{++vertical;});QObject::connect(view.verticalHeader(),&QHeaderView::geometriesChanged,&observer,[&]{++verticalSettled;});
 auto columns=sheet.at("columnLayout");columns[3]["size"]=16.0;require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"columns",columns}}})),"change exactly one stored column width");
 require(horizontal==1&&vertical==0,"one-column layout change emits one horizontal resize and no unrelated row resizes");
 require(verticalSettled==0,"unchanged effective row default needs no geometry batch even at the minimum height");
 for(int i=0;i<7;++i){require(view.rowHeight(i)==40,"unchanged custom row heights are preserved");require(view.columnWidth(i)==(i==3?112:98),"only the changed column receives a new native width");}
 require(view.isRowHidden(7)&&view.isColumnHidden(7)&&view.rowSpan(0,0)==2&&view.columnSpan(0,0)==2,"incremental width synchronization preserves hidden sections and merged cells");require(session.snapshotView().at("sheets")[0].at("cells")==before.at("sheets")[0].at("cells")&&view.currentIndex()==current&&view.selectionModel()->selection()==selected,"layout synchronization preserves inputs, formulas, current cell and selection");
 horizontal=vertical=0;require(session.undo(),"single column width has one Undo");model.refreshAfterChange();require(horizontal==1&&vertical==0&&session.snapshotView().at("sheets")==before.at("sheets"),"Undo restores only that column without resetting unrelated sections");
}
void batchedDefaultGeometry(){
 auto book=WorkbookSession().snapshot();auto& sheet=book["sheets"][0];sheet["rowLayout"]=Json::array({{{"index",4},{"size",30.0}},{{"index",5},{"size",25.0},{"hidden",true}}});sheet["columnLayout"]=Json::array({{{"index",4},{"size",14.0}},{{"index",6},{"size",18.0},{"hidden",true}}});sheet["mergedRanges"]=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",2}}});
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.verticalHeader()->setDefaultSectionSize(20);view.horizontalHeader()->setDefaultSectionSize(64);view.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view.setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);view.setModel(&model);view.resize(800,400);view.show();view.setCurrentIndex(model.index(6,1));QApplication::processEvents();view.setCopyRange(model.sheetId(),QItemSelectionRange(model.index(6,1),model.index(7,2)));auto* copy=view.viewport()->findChild<QWidget*>("worksheet.copyBorder");require(copy,"prepare a real copy outline for layout synchronization");const auto copyBefore=copy->property("sourceRect").toRect();const auto before=session.snapshot();const auto current=view.currentIndex();const auto selection=view.selectionModel()->selection();const int verticalMaximum=view.verticalScrollBar()->maximum(),horizontalMaximum=view.horizontalScrollBar()->maximum();
 int horizontal=0,vertical=0,hSettled=0,vSettled=0;bool settled=true;QObject observer;
 QObject::connect(view.horizontalHeader(),&QHeaderView::sectionResized,&observer,[&]{++horizontal;});QObject::connect(view.verticalHeader(),&QHeaderView::sectionResized,&observer,[&]{++vertical;});
 auto coherent=[&]{return view.rowHeight(3)==40&&view.columnWidth(3)==98&&view.rowHeight(6)==24&&view.columnWidth(1)==84&&view.verticalScrollBar()->maximum()>verticalMaximum&&view.horizontalScrollBar()->maximum()>horizontalMaximum;};
 QObject::connect(view.horizontalHeader(),&QHeaderView::geometriesChanged,&observer,[&]{++hSettled;settled=settled&&coherent();});QObject::connect(view.verticalHeader(),&QHeaderView::geometriesChanged,&observer,[&]{++vSettled;settled=settled&&coherent();});
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"defaultRowHeight",18.0},{"defaultColumnWidth",12.0}}})),"change sparse defaults without materializing rows or columns");
 require(horizontal==0&&vertical==0,"default dimensions do not publish per-section callbacks for the whole worksheet");require(hSettled==1&&vSettled==1&&settled,"each changed header publishes one settled geometry notification after scrollbar and custom-size restoration");
 require(coherent()&&view.isRowHidden(4)&&view.isColumnHidden(5)&&view.rowSpan(0,0)==2&&view.columnSpan(0,0)==2,"batched default changes preserve custom dimensions, hidden sections and merged cells");require(copy->isVisible()&&copy->property("sourceRect").toRect()!=copyBefore&&view.currentIndex()==current&&view.selectionModel()->selection()==selection,"copy outline follows settled default geometry without moving the selection");
 QObject::disconnect(view.horizontalHeader(),nullptr,&observer,nullptr);QObject::disconnect(view.verticalHeader(),nullptr,&observer,nullptr);require(session.undo(),"default dimensions remain one Undo");model.refreshAfterChange();require(session.snapshotView().at("sheets")==before.at("sheets")&&view.rowHeight(6)==20&&view.columnWidth(1)==64&&view.rowHeight(3)==40&&view.columnWidth(3)==98&&view.isRowHidden(4)&&view.isColumnHidden(5),"Undo restores sparse defaults, custom dimensions and hidden state");
}
void sparseDefaultPerformance(){
 auto book=WorkbookSession().snapshot();auto& rows=book["sheets"][0]["rowLayout"]=Json::array();for(int i=1;i<=5000;++i)rows.push_back({{"index",i},{"size",30.0}});
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.setUpdatesEnabled(false);view.verticalHeader()->setDefaultSectionSize(20);view.setModel(&model);view.setUpdatesEnabled(true);
 QElapsedTimer timer;timer.start();require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"defaultRowHeight",15.75}}})),"change a default with 5000 custom rows");const auto milliseconds=timer.elapsed();std::cout<<"Sparse default height with 5000 custom rows: "<<milliseconds<<" ms\n";
 require(milliseconds<1000,"default-height synchronization stays responsive with 5000 custom rows");require(view.rowHeight(0)==40&&view.rowHeight(4999)==40&&view.rowHeight(5001)==21&&view.verticalHeader()->updatesEnabled(),"fast default synchronization retains custom and default dimensions and restores enabled updates");
 view.setUpdatesEnabled(false);require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"defaultRowHeight",18.0}}})),"change defaults while ancestor updates are disabled");require(!view.verticalHeader()->updatesEnabled()&&!view.horizontalHeader()->updatesEnabled(),"batch preserves inherited disabled updates");view.setUpdatesEnabled(true);require(view.verticalHeader()->updatesEnabled()&&view.horizontalHeader()->updatesEnabled(),"headers inherit enabled updates again without becoming explicitly disabled");
 view.verticalHeader()->setUpdatesEnabled(false);require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"defaultRowHeight",15.75}}})),"change defaults while header updates are explicitly disabled");require(!view.verticalHeader()->updatesEnabled()&&view.horizontalHeader()->updatesEnabled(),"batch preserves explicitly disabled header updates");
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 auto book=WorkbookSession().snapshot();book["sheets"][0]["mergedRanges"]=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",3}}});
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.resize(800,400);view.show();QApplication::processEvents();
 require(view.rowSpan(0,0)==2&&view.columnSpan(0,0)==3,"native merged ranges render as one worksheet cell");
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model.sheetId().toStdString()},{"mergedRanges",Json::array()}}})),"unmerge layout transaction");
 require(view.rowSpan(0,0)==1&&view.columnSpan(0,0)==1,"unmerge updates the visible grid immediately");
 require(session.undo(),"undo unmerge");model.refresh();require(view.rowSpan(0,0)==2&&view.columnSpan(0,0)==3,"undo restores the rendered merged region");
 auto dimensionBook=WorkbookSession().snapshot();dimensionBook["sheets"][0]["rowLayout"]=Json::array({{{"index",2},{"size",27.0}},{{"index",3},{"hidden",true}}});dimensionBook["sheets"][0]["columnLayout"]=Json::array({{{"index",2},{"size",10.0}},{{"index",4},{"hidden",true}}});
 WorkbookSession dimensions(dimensionBook);WorksheetModel dimensionModel(dimensions);WorksheetView dimensionView;dimensionView.verticalHeader()->setDefaultSectionSize(24);dimensionView.horizontalHeader()->setDefaultSectionSize(112);dimensionView.setModel(&dimensionModel);
 require(dimensionView.rowHeight(1)==qRound(27.0*dimensionView.logicalDpiY()/72.0)&&dimensionView.isRowHidden(2),"native row height and hidden state render in the grid");
 const int digit=SpreadsheetNormalDigitWidth;
 require(dimensionView.columnWidth(1)==10*digit&&dimensionView.isColumnHidden(3),"native stored widths and hidden state render in the grid without duplicate padding");
 require(dimensionModel.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",dimensionModel.sheetId().toStdString()},{"rows",Json::array()},{"columns",Json::array()}}})),"reset dimensions transaction");
 require(!dimensionView.isRowHidden(2)&&!dimensionView.isColumnHidden(3)&&dimensionView.rowHeight(1)==24&&dimensionView.columnWidth(1)==112,"cleared native dimensions restore default sizes and visibility");
 incrementalDimensions();
 incrementalDimensions(0.1);
 batchedDefaultGeometry();
 sparseDefaultPerformance();
 std::cout<<"Worksheet layout UI tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
