#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include <QApplication>
#include <QHeaderView>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
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
 std::cout<<"Worksheet layout UI tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
