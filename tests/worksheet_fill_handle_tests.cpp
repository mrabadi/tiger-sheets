#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/worksheet_selection.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTest>
#include <iostream>
#include <stdexcept>
#include <array>
#include <vector>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 WorkbookSession session;
 WorksheetModel model{session};
 WorksheetView view;
 Fixture(){view.setModel(&model);view.horizontalHeader()->setDefaultSectionSize(72);view.resize(1080,600);view.show();QApplication::processEvents();}
 void startDrag(int row,int col){view.setCurrentIndex(model.index(row,col));QApplication::processEvents();const auto handle=view.visualRect(model.index(row,col)).bottomRight()-QPoint(1,1);QTest::mousePress(view.viewport(),Qt::LeftButton,Qt::NoModifier,handle);}
 void startRangeDrag(int top,int left,int bottom,int right,bool reverse=false){const auto first=view.visualRect(model.index(reverse?bottom:top,reverse?right:left)).center(),last=view.visualRect(model.index(reverse?top:bottom,reverse?left:right)).center();QTest::mousePress(view.viewport(),Qt::LeftButton,Qt::NoModifier,first);QMouseEvent move(QEvent::MouseMove,QPointF(last),QPointF(view.viewport()->mapToGlobal(last)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view.viewport(),&move);QTest::mouseRelease(view.viewport(),Qt::LeftButton,Qt::NoModifier,last);QApplication::processEvents();const auto selected=coalescedWorksheetSelection(view.selectionModel()->selection());require(selected.size()==1&&selected.front().top()==top&&selected.front().left()==left&&selected.front().bottom()==bottom&&selected.front().right()==right,"native pointer drag selects the complete source rectangle");require(view.currentIndex()==model.index(reverse?bottom:top,reverse?right:left),"source selection retains its original active corner");const auto handle=view.visualRect(model.index(bottom,right)).bottomRight()-QPoint(1,1);QTest::mousePress(view.viewport(),Qt::LeftButton,Qt::NoModifier,handle);}
 void moveDrag(int row,int col){const auto point=view.visualRect(model.index(row,col)).center();QMouseEvent move(QEvent::MouseMove,QPointF(point),QPointF(view.viewport()->mapToGlobal(point)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view.viewport(),&move);}
 void releaseDrag(int row,int col){QTest::mouseRelease(view.viewport(),Qt::LeftButton,Qt::NoModifier,view.visualRect(model.index(row,col)).center());QApplication::processEvents();}
};
void multipleRowsFillAcross(){
 Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();Json operations=Json::array();
 for(int row=2;row<=4;++row)for(int col=2;col<=3;++col)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row+1},{"col",col+1},{"input","="+columnName(col)+std::to_string(row+1)+"+$A$1+"+std::to_string(row*10+col)},{"style",{{"bold",col==2},{"italic",row==3},{"numberFormat",row==2?"0.00":"0%"}}}});
 require(fixture.model.apply(operations),"seed a three-row, two-column formula block with distinct source styles");const auto before=fixture.session.snapshot();fixture.startRangeDrag(2,2,4,3);fixture.moveDrag(4,8);require(fixture.session.snapshot()==before,"multi-row horizontal fill is a preview until release");fixture.releaseDrag(4,8);
 for(int row=2;row<=4;++row)for(int col=4;col<=8;++col){const int source=2+(col-2)%2;require(fixture.model.raw(row,col).toStdString()=="="+columnName(col)+std::to_string(row+1)+"+$A$1+"+std::to_string(row*10+source),"horizontal fill extends every selected row using its corresponding source formula");require(fixture.model.cellStyle(row,col)==fixture.model.cellStyle(row,source),"horizontal block fill repeats each row's source column styles, including a partial final repeat");}
 for(int row=2;row<=4;++row)for(int col=2;col<=3;++col)require(fixture.model.raw(row,col).toStdString()=="="+columnName(col)+std::to_string(row+1)+"+$A$1+"+std::to_string(row*10+col),"horizontal fill leaves every source cell unchanged");require(fixture.model.raw(1,4).isEmpty()&&fixture.model.raw(5,4).isEmpty(),"horizontal fill keeps rows outside the selected block untouched");
 require(fixture.session.undo(),"multi-row fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores the complete multi-row extension");
}
void multipleColumnsFillDown(){
 Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();Json operations=Json::array();
 for(int row=2;row<=3;++row)for(int col=2;col<=4;++col)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row+1},{"col",col+1},{"input","=$A"+std::to_string(row+1)+"+"+columnName(col+1)+"$1+"+std::to_string(row*10+col)},{"style",{{"bold",row==2},{"italic",col==3},{"numberFormat",col==2?"0.00":"0%"}}}});
 require(fixture.model.apply(operations),"seed a two-row, three-column formula block with distinct source styles");const auto before=fixture.session.snapshot();fixture.startRangeDrag(2,2,3,4);fixture.moveDrag(8,4);require(fixture.session.snapshot()==before,"multi-column vertical fill is a preview until release");fixture.releaseDrag(8,4);
 for(int row=4;row<=8;++row)for(int col=2;col<=4;++col){const int source=2+(row-2)%2;require(fixture.model.raw(row,col).toStdString()=="=$A"+std::to_string(row+1)+"+"+columnName(col+1)+"$1+"+std::to_string(source*10+col),"vertical fill extends every selected column using its corresponding source formula");require(fixture.model.cellStyle(row,col)==fixture.model.cellStyle(source,col),"vertical block fill repeats each column's source row styles, including a partial final repeat");}
 for(int row=2;row<=3;++row)for(int col=2;col<=4;++col)require(fixture.model.raw(row,col).toStdString()=="=$A"+std::to_string(row+1)+"+"+columnName(col+1)+"$1+"+std::to_string(row*10+col),"vertical fill leaves every source cell unchanged");require(fixture.model.raw(4,1).isEmpty()&&fixture.model.raw(4,5).isEmpty(),"vertical fill keeps columns outside the selected block untouched");
 require(fixture.session.undo(),"multi-column fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores the complete multi-column extension");
}
void shortExtensionsRespectTheSelectedAxis(){
 for(const bool horizontal:{true,false})for(const bool reverse:{false,true}){
  Fixture fixture;const int bottom=horizontal?5:2,right=horizontal?2:5;Json operations=Json::array();const auto sheet=fixture.model.sheetId().toStdString();
  for(int row=2;row<=bottom;++row)for(int col=2;col<=right;++col)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row+1},{"col",col+1},{"input","source-"+std::to_string(row)+"-"+std::to_string(col)},{"style",{{"bold",row%2==0},{"italic",col%2==0}}}});
  require(fixture.model.apply(operations),"seed distinct source values for a tall or wide selection");const auto before=fixture.session.snapshot();const int targetRow=bottom+(horizontal?0:1),targetCol=right+(horizontal?1:0);fixture.startRangeDrag(2,2,bottom,right,reverse);fixture.moveDrag(targetRow,targetCol);fixture.releaseDrag(targetRow,targetCol);
  for(int row=2;row<=bottom;++row)for(int col=2;col<=right;++col){const int destinationRow=horizontal?row:targetRow,destinationCol=horizontal?targetCol:col;require(fixture.model.raw(destinationRow,destinationCol)==fixture.model.raw(row,col)&&fixture.model.cellStyle(destinationRow,destinationCol)==fixture.model.cellStyle(row,col),"a one-cell extension fills the complete perpendicular span regardless of source aspect ratio or active corner");}
  require(fixture.model.raw(bottom+1,right+1).isEmpty(),"a short extension never fills a diagonal cell outside its axis");require(fixture.session.undo(),"short range fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores a short extension from either active corner");
 }
}
void blocksFillLeftAndUp(){
 for(const bool horizontal:{true,false}){
  Fixture fixture;const int bottom=horizontal?3:4,right=horizontal?4:3;const auto sheet=fixture.model.sheetId().toStdString();Json operations=Json::array();
  for(int row=2;row<=bottom;++row)for(int col=2;col<=right;++col)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row+1},{"col",col+1},{"input","=J10+$A$1+"+std::to_string(row*10+col)},{"style",{{"bold",row%2==0},{"italic",col%2==0}}}});
  require(fixture.model.apply(operations),"seed an asymmetric block for leftward and upward repetition");const auto before=fixture.session.snapshot();fixture.startRangeDrag(2,2,bottom,right);fixture.moveDrag(horizontal?bottom:0,horizontal?0:right);fixture.releaseDrag(horizontal?bottom:0,horizontal?0:right);
  if(horizontal)for(int row=2;row<=bottom;++row)for(int col=0;col<=1;++col){require(fixture.model.raw(row,col).toStdString()=="=G10+$A$1+"+std::to_string(row*10+col+3),"leftward fill uses the trailing source columns and shifts references left");require(fixture.model.cellStyle(row,col)==fixture.model.cellStyle(row,col+3),"leftward fill retains the matching source style");}
  else for(int row=0;row<=1;++row)for(int col=2;col<=right;++col){require(fixture.model.raw(row,col).toStdString()=="=J7+$A$1+"+std::to_string((row+3)*10+col),"upward fill uses the trailing source rows and shifts references up");require(fixture.model.cellStyle(row,col)==fixture.model.cellStyle(row+3,col),"upward fill retains the matching source style");}
  require(fixture.session.undo(),"reverse-direction range fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores the complete leftward or upward extension");
 }
}
void rangeCancellationAndMergedDestinations(){
 for(const bool merged:{false,true}){
  Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();Json operations=Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",3},{"col",3},{"input","first"}},{{"type","set_cell"},{"sheet",sheet},{"row",4},{"col",4},{"input","last"}}});if(merged)operations.push_back({{"type","set_sheet_layout"},{"sheet",sheet},{"mergedRanges",Json::array({{{"row1",4},{"col1",7},{"row2",4},{"col2",8}}})}});
  require(fixture.model.apply(operations),"seed a range with a possible merged destination");const auto before=fixture.session.snapshot();const auto history=fixture.session.historyBytes();fixture.startRangeDrag(2,2,3,3);fixture.moveDrag(3,8);if(!merged)QTest::keyClick(&fixture.view,Qt::Key_Escape);fixture.releaseDrag(3,8);require(fixture.session.snapshot()==before&&fixture.session.historyBytes()==history,"Escape or a merged destination refuses the entire block fill without source changes, partial writes, or Undo history");
 }
 Fixture fixture;require(fixture.model.setData(fixture.model.index(2,2),"source",Qt::EditRole),"seed a block handle click");const auto before=fixture.session.snapshot();const auto history=fixture.session.historyBytes();fixture.startRangeDrag(2,2,3,3);fixture.releaseDrag(3,3);require(fixture.session.snapshot()==before&&fixture.session.historyBytes()==history,"clicking a block's handle without extending makes no edits or Undo entry");
}
void mergedBlocksAreRefusedEvenWithBlankCells(){
 for(const bool mergedSource:{true,false}){
  Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();require(fixture.model.apply(Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",3},{"col",3},{"input","anchor"}},{{"type","set_cell"},{"sheet",sheet},{"row",4},{"col",4},{"input","last"}},{{"type","set_sheet_layout"},{"sheet",sheet},{"mergedRanges",Json::array({{{"row1",3},{"col1",mergedSource?3:7},{"row2",3},{"col2",mergedSource?4:8}}})}}})),"seed a merged source or a destination whose covered source pattern is blank");
  const auto before=fixture.session.snapshot();const auto history=fixture.session.historyBytes();fixture.startRangeDrag(2,2,3,3);fixture.moveDrag(3,8);fixture.releaseDrag(3,8);require(fixture.session.snapshot()==before&&fixture.session.historyBytes()==history,mergedSource?"a source containing merged cells cannot be flattened into unmerged fill tiles":"a merged destination refuses a block fill even when its covered cell would receive a blank");
 }
}
void rangeDragScrollsAlongItsExtension(){
 for(const bool horizontal:{true,false}){
  Fixture fixture;fixture.view.resize(400,240);fixture.view.setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);fixture.view.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);QApplication::processEvents();const auto sheet=fixture.model.sheetId().toStdString();Json operations=Json::array();for(int row=2;row<=3;++row)for(int col=2;col<=3;++col)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row+1},{"col",col+1},{"input","source-"+std::to_string(row)+"-"+std::to_string(col)}});require(fixture.model.apply(operations),"seed a two-dimensional scrolling fill source");const auto before=fixture.session.snapshot();fixture.startRangeDrag(2,2,3,3);
  const auto corner=fixture.view.visualRect(fixture.model.index(3,3)).center();const QPoint outside(horizontal?fixture.view.viewport()->width()+18:corner.x(),horizontal?corner.y():fixture.view.viewport()->height()+18);QMouseEvent move(QEvent::MouseMove,QPointF(outside),QPointF(fixture.view.viewport()->mapToGlobal(outside)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(fixture.view.viewport(),&move);auto* along=horizontal?fixture.view.horizontalScrollBar():fixture.view.verticalScrollBar();auto* across=horizontal?fixture.view.verticalScrollBar():fixture.view.horizontalScrollBar();require(QTest::qWaitFor([&]{return along->value()>0;},2000)&&across->value()==0,"block fill scrolls only along the extended axis");require(fixture.session.snapshot()==before,"scrolling a block fill remains a preview");
  const QPoint endpoint(horizontal?fixture.view.viewport()->width()-8:corner.x(),horizontal?corner.y():fixture.view.viewport()->height()-8);const auto target=fixture.view.indexAt(endpoint);require(target.isValid()&&(horizontal?target.column()>3:target.row()>3),"scrolling exposes a cell beyond the entire source block");QTest::mouseRelease(fixture.view.viewport(),Qt::LeftButton,Qt::NoModifier,endpoint);QApplication::processEvents();if(horizontal)for(int row=2;row<=3;++row)require(fixture.model.raw(row,target.column()).toStdString()=="source-"+std::to_string(row)+"-"+std::to_string(2+(target.column()-2)%2),"horizontal edge scrolling fills every selected row through its endpoint");else for(int col=2;col<=3;++col)require(fixture.model.raw(target.row(),col).toStdString()=="source-"+std::to_string(2+(target.row()-2)%2)+"-"+std::to_string(col),"vertical edge scrolling fills every selected column through its endpoint");require(fixture.session.undo(),"scrolling block fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores a block fill after edge scrolling");
 }
}
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
void formattingFillsAcrossAndDown(){
 struct FormatCase {const char* format;const char* seed;std::array<const char*,4> displays;bool date;};
 const std::vector<FormatCase> formats={{"mmm-yy","45930",{"Oct-25","Nov-25","Dec-25","Jan-26"},true},{"0.00%","0.0625",{"12.50%","25.00%","50.00%","100.00%"},false},{"#,##0.000","617.25",{"1,234.500","2,469.000","4,938.000","9,876.000"},false},{"$#,##0.00;($#,##0.00);-","-617.25",{"($1,234.50)","($2,469.00)","($4,938.00)","($9,876.00)"},false}};
 for(const auto& format:formats)for(const bool horizontal:{true,false}){
  Fixture fixture;const auto sheet=fixture.model.sheetId().toStdString();const Json sourceStyle={{"numberFormat",format.format},{"bold",true},{"fill","#eaf4ee"},{"alignment","right"}};const auto sourceFormula=std::string(format.date?"=EOMONTH(":"=")+(horizontal?"B6":"C5")+(format.date?",1)":"*2");
  Json operations=Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",horizontal?6:5},{"col",horizontal?2:3},{"input",format.seed}},{{"type","set_cell"},{"sheet",sheet},{"row",6},{"col",3},{"input",sourceFormula},{"style",sourceStyle}}});
  for(int offset=1;offset<=3;++offset)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",6+(horizontal?0:offset)},{"col",3+(horizontal?offset:0)},{"input","999"},{"style",{{"numberFormat","0.0%"},{"italic",true},{"color","#ff0000"}}}});
  require(fixture.model.apply(operations),"seed formatted source and conflicting populated destinations");fixture.model.setCalculated(CalculationEngine().calculate(fixture.session.snapshotView()));require(fixture.model.data(fixture.model.index(5,2)).toString()==format.displays[0],"source formula displays its selected date, percent, decimal or negative-currency format");const auto before=fixture.session.snapshot();
  fixture.startDrag(5,2);fixture.moveDrag(5+(horizontal?0:3),2+(horizontal?3:0));require(fixture.session.snapshot()==before,"formatted fill remains a preview until the actual mouse release");fixture.releaseDrag(5+(horizontal?0:3),2+(horizontal?3:0));fixture.model.setCalculated(CalculationEngine().calculate(fixture.session.snapshotView()));
  for(int offset=0;offset<=3;++offset){const int row=5+(horizontal?0:offset),col=2+(horizontal?offset:0);const auto previous=horizontal?columnName(2+offset)+"6":"C"+std::to_string(5+offset);const auto expected=std::string(format.date?"=EOMONTH(":"=")+previous+(format.date?",1)":"*2");require(fixture.model.raw(row,col).toStdString()==expected,"formatted fill shifts the formula in the dragged direction");require(fixture.model.cellStyle(row,col)==sourceStyle,"formatted fill replaces destination formatting with the complete source style");require(fixture.model.data(fixture.model.index(row,col)).toString()==format.displays[offset],"drag-filled formulas display the source date, percent, decimal and currency choices after real calculation");}
  require(fixture.session.undo(),"formatted fill creates an undoable action");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores every destination formula, value and prior style after formatted fill");
 }
}
void inferredDateFormulaFormatFills(){
 Fixture fixture;require(fixture.model.setData(fixture.model.index(2,1),"9/30/2025",Qt::EditRole)&&fixture.model.setData(fixture.model.index(2,2),"=EOMONTH(B3,1)",Qt::EditRole),"enter a date and the next-month formula without manually setting a format");fixture.model.setCalculated(CalculationEngine().calculate(fixture.session.snapshotView()));require(fixture.model.numericValue(2,2).value_or(-1)==45961&&fixture.model.data(fixture.model.index(2,2)).toString()=="45961","source EOMONTH entry in General initially displays its numeric serial");require(fixture.model.apply(Json::array({{{"type","set_style"},{"sheet",fixture.model.sheetId().toStdString()},{"row",3},{"col",3},{"style",{{"numberFormat","m/d/yyyy"}}}}})),"apply the explicit source date formatting before filling");const auto style=fixture.model.cellStyle(2,2),before=fixture.session.snapshot();fixture.startDrag(2,2);fixture.moveDrag(2,5);fixture.releaseDrag(2,5);fixture.model.setCalculated(CalculationEngine().calculate(fixture.session.snapshotView()));const std::array<const char*,4> displays={"10/31/2025","11/30/2025","12/31/2025","1/31/2026"};
 for(int offset=0;offset<=3;++offset){require(fixture.model.raw(2,2+offset).toStdString()=="=EOMONTH("+columnName(2+offset)+"3,1)","inferred-date fill shifts each preceding-month reference");require(fixture.model.cellStyle(2,2+offset)==style&&fixture.model.data(fixture.model.index(2,2+offset)).toString()==displays[offset],"inferred source date format propagates through the filled monthly sequence");}
 require(fixture.session.undo(),"inferred-date fill is undoable");fixture.model.refresh();require(fixture.session.snapshot().at("sheets")==before.at("sheets"),"one Undo restores the inferred-date sequence's original cells and formatting");
}
void escapeCancelsDrag(){
 Fixture fixture;require(fixture.model.setData(fixture.model.index(5,2),"=SUM(C3:C5)",Qt::EditRole),"seed cancel fixture");const auto before=fixture.session.snapshot();
 fixture.startDrag(5,2);fixture.moveDrag(5,8);QTest::keyClick(&fixture.view,Qt::Key_Escape);fixture.releaseDrag(5,8);
 require(fixture.session.snapshot()==before,"Escape cancels a fill preview without writing the dragged cells or creating undo history");
}
void visibleHandleAndClickWithoutDragging(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(5,2));QApplication::processEvents();const auto point=fixture.view.visualRect(fixture.model.index(5,2)).bottomRight()+QPoint(1,1);
 require(fixture.view.viewport()->grab().toImage().pixelColor(point)==QColor("#217346"),"active cell exposes a visible bottom-right fill handle");const auto before=fixture.session.snapshot();fixture.startDrag(5,2);fixture.releaseDrag(5,2);
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
int main(int argc,char** argv){QApplication app(argc,argv);try{const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"multi_row",multipleRowsFillAcross},std::pair{"multi_column",multipleColumnsFillDown},std::pair{"short",shortExtensionsRespectTheSelectedAxis},std::pair{"reverse",blocksFillLeftAndUp},std::pair{"range_protected",rangeCancellationAndMergedDestinations},std::pair{"merged_blocks",mergedBlocksAreRefusedEvenWithBlankCells},std::pair{"range_scroll",rangeDragScrollsAlongItsExtension},std::pair{"horizontal",horizontalFormulas},std::pair{"vertical",verticalMixedReferences},std::pair{"formats",formattingFillsAcrossAndDown},std::pair{"inferred_date",inferredDateFormulaFormatFills},std::pair{"escape",escapeCancelsDrag},std::pair{"handle",visibleHandleAndClickWithoutDragging},std::pair{"protected",previewAndMergedDestinationRemainProtected},std::pair{"scroll",edgeDragScrollsToMoreColumns}})if(selected.empty()||selected==test.first)test.second();std::cout<<"Worksheet fill handle tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
