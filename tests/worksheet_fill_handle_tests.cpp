#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 WorkbookSession session;
 WorksheetModel model{session};
 WorksheetView view;
 Fixture(){view.setModel(&model);view.horizontalHeader()->setDefaultSectionSize(72);view.resize(1080,600);view.show();QApplication::processEvents();}
 void startDrag(int row,int col){view.setCurrentIndex(model.index(row,col));QApplication::processEvents();const auto handle=view.visualRect(model.index(row,col)).bottomRight()-QPoint(1,1);QTest::mousePress(view.viewport(),Qt::LeftButton,Qt::NoModifier,handle);}
 void moveDrag(int row,int col){const auto point=view.visualRect(model.index(row,col)).center();QMouseEvent move(QEvent::MouseMove,QPointF(point),QPointF(view.viewport()->mapToGlobal(point)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view.viewport(),&move);}
 void releaseDrag(int row,int col){QTest::mouseRelease(view.viewport(),Qt::LeftButton,Qt::NoModifier,view.visualRect(model.index(row,col)).center());QApplication::processEvents();}
};
void horizontalFormulas(){
 Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();
 require(fixture.model.apply(Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",6},{"col",3},{"input","=B6*(1+$B$2)"},{"style",{{"numberFormat","#,##0"},{"bold",true}}}}})),"seed revenue formula");
 const auto before=fixture.session.snapshot();fixture.startDrag(5,2);fixture.moveDrag(5,13);
 require(fixture.session.snapshot()==before,"drag preview does not change the workbook before release");fixture.releaseDrag(5,13);
 for(int col=3;col<=13;++col){const auto expected="="+columnName(col)+"6*(1+$B$2)";require(fixture.model.raw(5,col).toStdString()==expected,"dragging the fill handle right shifts relative columns and preserves absolute references");require(fixture.model.cellStyle(5,col)==fixture.model.cellStyle(5,2),"drag fill carries source number formatting and bold style");}
 require(fixture.model.raw(5,2)=="=B6*(1+$B$2)","drag fill preserves the source formula");
 require(fixture.session.undo(),"fill handle action is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one undo restores the entire dragged range");
}
void verticalMixedReferences(){
 Fixture fixture;require(fixture.model.setData(fixture.model.index(5,2),"=$B6*C$4+$B$2",Qt::EditRole),"seed mixed reference formula");const auto before=fixture.session.snapshot();
 fixture.startDrag(5,2);fixture.moveDrag(6,2);fixture.releaseDrag(6,2);
 require(fixture.model.raw(6,2)=="=$B7*C$4+$B$2","dragging one row down shifts relative rows while preserving locked rows and columns");
 require(fixture.model.raw(5,2)=="=$B6*C$4+$B$2"&&fixture.model.raw(6,3).isEmpty(),"vertical fill preserves its source and neighboring cells");
 require(fixture.session.undo(),"one-row fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one undo restores one-row fill");
}
void escapeCancelsDrag(){
 Fixture fixture;require(fixture.model.setData(fixture.model.index(5,2),"=SUM(C3:C5)",Qt::EditRole),"seed cancel fixture");const auto before=fixture.session.snapshot();
 fixture.startDrag(5,2);fixture.moveDrag(5,8);QTest::keyClick(&fixture.view,Qt::Key_Escape);fixture.releaseDrag(5,8);
 require(fixture.session.snapshot()==before,"Escape cancels a fill preview without writing the dragged cells or creating undo history");
}
void visibleHandleAndClickWithoutDragging(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(5,2));QApplication::processEvents();const auto point=fixture.view.visualRect(fixture.model.index(5,2)).bottomRight()-QPoint(3,3);
 require(fixture.view.viewport()->grab().toImage().pixelColor(point)==QColor("#245648"),"active cell exposes a visible bottom-right fill handle");const auto before=fixture.session.snapshot();fixture.startDrag(5,2);fixture.releaseDrag(5,2);
 require(fixture.session.snapshot()==before&&!fixture.session.canUndo(),"clicking the handle without extending it does not change the workbook or undo history");
}
void previewAndMergedDestinationRemainProtected(){
 Fixture fixture;require(fixture.model.setData(fixture.model.index(5,2),"=C3*C4",Qt::EditRole),"seed protected fill fixture");auto before=fixture.session.snapshot();fixture.model.showPreview(before);fixture.startDrag(5,2);fixture.moveDrag(5,8);fixture.releaseDrag(5,8);
 require(fixture.session.snapshot()==before,"fill handle cannot write while a workbook proposal is being previewed");fixture.model.refresh();
 require(fixture.model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet",fixture.model.sheetId().toStdString()},{"mergedRanges",Json::array({{{"row1",6},{"col1",7},{"row2",6},{"col2",8}}})}}})),"seed merged destination");before=fixture.session.snapshot();fixture.startDrag(5,2);fixture.moveDrag(5,10);fixture.releaseDrag(5,10);
 require(fixture.session.snapshot()==before,"a fill crossing merged interiors refuses atomically without partially filling earlier cells");
}
void edgeDragScrollsToMoreColumns(){
 Fixture fixture;fixture.view.resize(400,400);fixture.view.setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);QApplication::processEvents();require(fixture.model.setData(fixture.model.index(5,2),"=C3*C4",Qt::EditRole),"seed scrolling fill fixture");const auto before=fixture.session.snapshot();fixture.startDrag(5,2);
 const int y=fixture.view.visualRect(fixture.model.index(5,2)).center().y();const QPoint outside(fixture.view.viewport()->width()+18,y);QMouseEvent move(QEvent::MouseMove,QPointF(outside),QPointF(fixture.view.viewport()->mapToGlobal(outside)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(fixture.view.viewport(),&move);
 require(QTest::qWaitFor([&]{return fixture.view.horizontalScrollBar()->value()>0;},2000),"holding a dragged fill handle at the viewport edge scrolls to more columns");
 require(fixture.session.snapshot()==before,"edge scrolling remains an uncommitted fill preview");const QPoint endpoint(fixture.view.viewport()->width()-8,y);const auto target=fixture.view.indexAt(endpoint);require(target.isValid()&&target.column()>2,"scrolled endpoint identifies a later column");QTest::mouseRelease(fixture.view.viewport(),Qt::LeftButton,Qt::NoModifier,endpoint);QApplication::processEvents();
 require(fixture.model.raw(5,target.column()).toStdString()==shiftFormula("=C3*C4",0,target.column()-2),"releasing after edge scrolling fills through the newly visible endpoint");
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"horizontal",horizontalFormulas},std::pair{"vertical",verticalMixedReferences},std::pair{"escape",escapeCancelsDrag},std::pair{"handle",visibleHandleAndClickWithoutDragging},std::pair{"protected",previewAndMergedDestinationRemainProtected},std::pair{"scroll",edgeDragScrollsToMoreColumns}})if(selected.empty()||selected==test.first)test.second();std::cout<<"Worksheet fill handle tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
