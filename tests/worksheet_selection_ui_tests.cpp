#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QLineEdit>
#include <QLabel>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPicture>
#include <QProxyStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct PaintedRect {QRectF rect;QPen pen;QBrush brush;};
class RecordingEngine final:public QPaintEngine {
public:
 RecordingEngine():QPaintEngine(QPaintEngine::AllFeatures){}
 bool begin(QPaintDevice*)override{return true;}bool end()override{return true;}Type type()const override{return User;}
 void updateState(const QPaintEngineState& value)override{pen=value.pen();brush=value.brush();transform=value.transform();}
 void drawRects(const QRectF* values,int count)override{for(int i=0;i<count;++i)rects.push_back({transform.mapRect(values[i]),pen,brush});}
 void drawRects(const QRect* values,int count)override{for(int i=0;i<count;++i){const QRectF value(values[i]);drawRects(&value,1);}}
 void drawPath(const QPainterPath& value)override{if(value.elementCount()==5)rects.push_back({transform.mapRect(value.boundingRect()),pen,brush});}
 void drawPolygon(const QPointF*,int,PolygonDrawMode)override{}
 void drawPolygon(const QPoint*,int,PolygonDrawMode)override{}
 void drawLines(const QLineF*,int)override{}
 void drawLines(const QLine*,int)override{}
 void drawEllipse(const QRectF&)override{}
 void drawEllipse(const QRect&)override{}
 void drawPoints(const QPointF*,int)override{}
 void drawPoints(const QPoint*,int)override{}
 void drawPixmap(const QRectF&,const QPixmap&,const QRectF&)override{}
 void drawImage(const QRectF&,const QImage&,const QRectF&,Qt::ImageConversionFlags)override{}
 void drawTiledPixmap(const QRectF&,const QPixmap&,const QPointF&)override{}
 void drawTextItem(const QPointF&,const QTextItem&)override{}
 std::vector<PaintedRect> rects;QPen pen;QBrush brush;QTransform transform;
};
class RecordingDevice final:public QPaintDevice {
public:
 explicit RecordingDevice(QSize size):size_(size){}QPaintEngine* paintEngine()const override{return const_cast<RecordingEngine*>(&engine);}mutable RecordingEngine engine;
protected:
 int metric(PaintDeviceMetric metric)const override{switch(metric){case PdmWidth:return size_.width();case PdmHeight:return size_.height();case PdmDepth:return 32;case PdmDpiX:case PdmDpiY:case PdmPhysicalDpiX:case PdmPhysicalDpiY:return 96;case PdmDevicePixelRatio:return 1;case PdmDevicePixelRatioScaled:return 65536;default:return 0;}}
private:QSize size_;
};
class RecordingStyle final:public QProxyStyle {
public:
 mutable std::vector<QStyleOptionViewItem> cells;
 void drawControl(ControlElement element,const QStyleOption* option,QPainter* painter,const QWidget* widget=nullptr)const override{if(element==CE_ItemViewItem)if(const auto* cell=qstyleoption_cast<const QStyleOptionViewItem*>(option))cells.push_back(*cell);QProxyStyle::drawControl(element,option,painter,widget);}
};
struct Fixture {
 RecordingStyle style;WorkbookSession session;WorksheetModel model{session};WorksheetView view;
 Fixture(){view.setModel(&model);view.setStyle(&style);auto palette=view.palette();palette.setColor(QPalette::Highlight,QColor("#dfece3"));palette.setColor(QPalette::HighlightedText,QColor("#202c33"));view.setPalette(palette);view.resize(1000,600);view.horizontalHeader()->setDefaultSectionSize(112);view.verticalHeader()->setDefaultSectionSize(24);view.setSelectionMode(QAbstractItemView::ExtendedSelection);view.show();view.setFocus();QApplication::processEvents();}
 std::vector<PaintedRect> paint(){style.cells.clear();QPicture picture;view.viewport()->render(&picture);RecordingDevice device(view.viewport()->size());QPainter painter(&device);require(picture.play(&painter),"replay native paint commands without a screenshot");painter.end();return device.engine.rects;}
 void select(int row1,int col1,int row2,int col2){const auto first=model.index(row1,col1),last=model.index(row2,col2);view.setCurrentIndex(first);view.selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);QApplication::processEvents();}
};
bool borderAt(const std::vector<PaintedRect>& paint,const QRect& rect){for(const auto& draw:paint)if(draw.pen.style()!=Qt::NoPen&&draw.pen.color()==QColor("#217346")&&draw.pen.widthF()==2&&draw.brush.style()==Qt::NoBrush&&draw.rect==QRectF(rect))return true;return false;}
bool hasFillHandle(const std::vector<PaintedRect>& paint){for(const auto& draw:paint)if(draw.pen.style()==Qt::NoPen&&draw.brush.color()==QColor("#217346")&&draw.rect.size()==QSizeF(5,5))return true;return false;}
void dragHeader(QHeaderView* header,int first,int last){const bool horizontal=header->orientation()==Qt::Horizontal;auto center=[&](int section){const int along=header->sectionViewportPosition(section)+header->sectionSize(section)/2;return horizontal?QPoint(along,header->viewport()->height()/2):QPoint(header->viewport()->width()/2,along);};const auto start=center(first),end=center(last);QTest::mousePress(header->viewport(),Qt::LeftButton,Qt::NoModifier,start);QMouseEvent move(QEvent::MouseMove,end,header->viewport()->mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(header->viewport(),&move);QTest::mouseRelease(header->viewport(),Qt::LeftButton,Qt::NoModifier,end);QApplication::processEvents();}
void selectedCellsRetainFormatting(){
 Fixture f;const auto sheet=f.model.sheetId().toStdString();require(f.model.apply(Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",2},{"col",2},{"input","Formatted"},{"style",{{"fill","#1478be"},{"color","#ffffff"},{"bold",true},{"italic",true},{"numberFormat","0.00"}}}}})),"seed cell colors and font");const auto before=f.session.snapshot();f.select(1,0,2,3);f.paint();const auto target=f.model.index(1,1);bool found=false;
 for(const auto& cell:f.style.cells)if(cell.index==target){found=true;const bool selected=cell.state&QStyle::State_Selected;const auto background=selected?cell.palette.color(QPalette::Highlight):cell.backgroundBrush.color();const auto foreground=cell.palette.color(selected?QPalette::HighlightedText:QPalette::Text);require(background.blue()>background.red()*2&&background.blue()>100,"selection tint retains the cell's blue fill instead of replacing it with a flat selection color");require(foreground==QColor("#ffffff"),"selected text retains its explicit white font color");require(cell.font.bold()&&cell.font.italic(),"selection retains font formatting");}
 require(found,"native cell delegate paints the selected formatted cell");require(f.session.snapshot()==before,"selecting cells never mutates their stored formatting or history");
}
void selectedRangeHasOuterBorder(){
 Fixture f;f.select(1,1,1,1);const auto active=f.view.visualRect(f.model.index(1,1)).adjusted(-1,-1,1,1);require(borderAt(f.paint(),active),"vector paint recorder observes the existing single-cell border");f.select(1,1,3,3);const auto paint=f.paint();const auto bounds=f.view.visualRect(f.model.index(1,1)).united(f.view.visualRect(f.model.index(3,3))).adjusted(-1,-1,1,1);require(borderAt(paint,bounds),"a rectangular selection paints one outer border around its full range");require(!borderAt(paint,active),"a multi-cell selection does not leave a separate active-cell border inside it");
}
void headerDragDoesNotLeaveACellHandle(bool columns){
 Fixture f;f.view.setCurrentIndex(f.model.index(12,6));const auto before=f.session.snapshot();dragHeader(columns?f.view.horizontalHeader():f.view.verticalHeader(),1,3);const auto selection=f.view.selectionModel()->selection();require(selection.size()==1,"header drag creates one contiguous selection rather than stray cells");const auto& range=selection.front();require(columns?(range.left()==1&&range.right()==3&&range.top()==0&&range.bottom()==f.model.rowCount()-1):(range.top()==1&&range.bottom()==3&&range.left()==0&&range.right()==f.model.columnCount()-1),"native header drag selects exactly the intended full columns or rows");require(!hasFillHandle(f.paint()),"selecting whole rows or columns does not paint a stray single-cell fill handle");require(f.session.snapshot()==before,"header drag preserves workbook data, formatting and history");
}
class PaintObserver final:public QObject {
public:QRegion painted;
protected:bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::Paint)painted+=static_cast<QPaintEvent*>(event)->region();return false;}
};
void extendingSelectionRepaintsItsOldBorder(){
 Fixture f;f.select(1,1,3,3);QApplication::processEvents();const auto bounds=f.view.visualRect(f.model.index(1,1)).united(f.view.visualRect(f.model.index(3,3))).adjusted(-1,-1,1,1);PaintObserver observer;f.view.viewport()->installEventFilter(&observer);f.view.selectionModel()->select(QItemSelection(f.model.index(1,1),f.model.index(4,3)),QItemSelectionModel::ClearAndSelect);QApplication::processEvents();require(observer.painted.contains(QPoint(bounds.left()+20,bounds.bottom())),"extending a selection repaints its previous bottom edge so no interior border remains");
}
void currentCellOutsideSelectionHasNoDecoration(){
 Fixture f;f.select(1,1,3,3);const auto outside=f.model.index(12,6);f.view.selectionModel()->setCurrentIndex(outside,QItemSelectionModel::NoUpdate);require(!f.view.selectionModel()->isSelected(outside),"outside current cell is not a selected cell");const auto paint=f.paint();require(!borderAt(paint,f.view.visualRect(outside).adjusted(-1,-1,1,1))&&!hasFillHandle(paint),"a current cell outside the selection has no border or fill handle");for(const auto& cell:f.style.cells)if(cell.index==outside)require(!(cell.state&QStyle::State_HasFocus),"native delegate does not request a stray focus frame outside the selected range");
}
void mergedSelectionHasOneOuterBorder(){
 Fixture f;f.view.setSpan(1,0,1,6);f.view.setSpan(5,0,1,3);const auto anchor=f.model.index(1,0),end=f.model.index(7,8);const auto start=f.view.visualRect(anchor).center(),finish=f.view.visualRect(end).center();QTest::mousePress(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,start);QMouseEvent move(QEvent::MouseMove,finish,f.view.viewport()->mapToGlobal(finish),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.view.viewport(),&move);QTest::mouseRelease(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();require(f.view.selectionModel()->selection().size()>1,"native merged-cell drag produces the split selection ranges from the source workflow");const auto paint=f.paint();const auto bounds=f.view.visualRect(anchor).united(f.view.visualRect(end)).adjusted(-1,-1,1,1);require(borderAt(paint,bounds),"a contiguous merged selection paints one outer border around its full union");const auto internal=f.view.visualRect(anchor).adjusted(-1,-1,1,1);require(!borderAt(paint,internal),"merged cells inside the selected rectangle do not retain separate green outlines");
}
void disjointSelectionsKeepSeparateBorders(){
 Fixture f;f.select(1,1,2,2);f.view.selectionModel()->select(QItemSelection(f.model.index(4,4),f.model.index(5,5)),QItemSelectionModel::Select);QApplication::processEvents();const auto paint=f.paint();const auto first=f.view.visualRect(f.model.index(1,1)).united(f.view.visualRect(f.model.index(2,2))).adjusted(-1,-1,1,1),second=f.view.visualRect(f.model.index(4,4)).united(f.view.visualRect(f.model.index(5,5))).adjusted(-1,-1,1,1);require(borderAt(paint,first)&&borderAt(paint,second),"genuinely disjoint selections retain both individual outer borders");require(!borderAt(paint,first.united(second)),"selection union must not draw a false rectangle across unselected gaps");
}
void draggingPastItsAnchorPreservesTheViewport(){
 Fixture f;f.view.verticalHeader()->setDefaultSectionSize(21);f.view.setVerticalScrollMode(QAbstractItemView::ScrollPerItem);const auto anchor=f.model.index(97,5),endpoint=f.model.index(121,5);f.view.setCurrentIndex(anchor);f.view.scrollTo(anchor,QAbstractItemView::PositionAtTop);QApplication::processEvents();const auto before=f.session.snapshot();
 QTest::mousePress(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,f.view.visualRect(anchor).center());f.view.verticalScrollBar()->setValue(108);QApplication::processEvents();const int scrolled=f.view.verticalScrollBar()->value();const auto finish=f.view.visualRect(endpoint).center();require(!f.view.viewport()->rect().intersects(f.view.visualRect(anchor))&&f.view.viewport()->rect().contains(finish),"drag fixture exposes the endpoint while its original anchor is offscreen");
 QMouseEvent move(QEvent::MouseMove,finish,f.view.viewport()->mapToGlobal(finish),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.view.viewport(),&move);QApplication::processEvents();require(f.view.verticalScrollBar()->value()==scrolled&&f.view.viewport()->rect().contains(f.view.visualRect(endpoint).center()),"a held selection retains the user-scrolled viewport and visible release endpoint instead of following its old anchor");
 QTest::mouseRelease(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();const auto ranges=f.view.selectionModel()->selection();require(f.view.verticalScrollBar()->value()==scrolled&&f.view.currentIndex()==anchor&&ranges.size()==1&&ranges.front().top()==97&&ranges.front().bottom()==121,"release retains the scrolled viewport, original active anchor and exact 25-row range");require(f.session.snapshot()==before,"long pointer selection does not edit workbook data, formatting or history");
}
void offscreenAnchorShowsFloatingRangeDimensions(){
 Fixture f;QLineEdit name(&f.view);name.setObjectName("formula.name");f.view.verticalHeader()->setDefaultSectionSize(21);f.view.setVerticalScrollMode(QAbstractItemView::ScrollPerItem);const auto anchor=f.model.index(97,5),endpoint=f.model.index(121,5);f.view.setCurrentIndex(anchor);f.view.scrollTo(anchor,QAbstractItemView::PositionAtTop);QApplication::processEvents();const auto before=f.session.snapshot();
 QTest::mousePress(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,f.view.visualRect(anchor).center());f.view.verticalScrollBar()->setValue(108);QApplication::processEvents();const auto finish=f.view.visualRect(endpoint).center();QMouseEvent move(QEvent::MouseMove,finish,f.view.viewport()->mapToGlobal(finish),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.view.viewport(),&move);QApplication::processEvents();auto* hint=f.view.findChild<QLabel*>("worksheet.selectionRangeHint");
 require(name.text().isEmpty()&&hint&&hint->isVisible()&&hint->text()=="25R x 1C","a held selection with its anchor offscreen clears the Name Box and shows floating range dimensions");require(f.view.viewport()->rect().contains(hint->geometry()),"floating range dimensions stay inside the scrolled worksheet viewport");
 QTest::mouseRelease(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();require(name.text()=="F98"&&!hint->isVisible(),"release restores the active address and dismisses the transient range hint");f.view.scrollTo(anchor,QAbstractItemView::PositionAtTop);QApplication::processEvents();const auto shortEnd=f.view.visualRect(f.model.index(98,5)).center();QTest::mousePress(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,f.view.visualRect(anchor).center());QMouseEvent shortMove(QEvent::MouseMove,shortEnd,f.view.viewport()->mapToGlobal(shortEnd),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.view.viewport(),&shortMove);QApplication::processEvents();require(name.text()=="2R x 1C"&&!hint->isVisible(),"an on-screen anchor retains the ordinary Name Box dimensions instead of a floating hint");QTest::mouseRelease(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,shortEnd);require(f.session.snapshot()==before,"range dimension surfaces never change workbook contents or history");
}
void sourceCellOutlinePixels(){
 Fixture f;f.view.horizontalHeader()->setDefaultSectionSize(64);f.view.verticalHeader()->setDefaultSectionSize(20);f.select(1,1,1,1);const auto before=f.session.snapshot();const auto rect=f.view.visualRect(f.model.index(1,1));const auto image=f.view.viewport()->grab().toImage();const auto green=QColor("#217346");
 for(int x=rect.left()+8;x<rect.right()-8;++x)for(const int y:{rect.top()-2,rect.top()-1,rect.bottom()+1,rect.bottom()+2})require(image.pixelColor(x,y)==green,"source active-cell stroke covers the gridline and its outside neighbor, rather than insetting into the cell");
 require(image.pixelColor(rect.center())!=green&&f.session.snapshot()==before,"source outline painting leaves cell content, selection and history intact");f.view.setShowGrid(false);QApplication::processEvents();const auto withoutGrid=f.view.viewport()->grab().toImage();for(int x=rect.left()+8;x<rect.right()-8;++x)for(const int y:{rect.top()-2,rect.top()-1,rect.bottom()+1,rect.bottom()+2})require(withoutGrid.pixelColor(x,y)==green,"hiding gridlines leaves the active outline at the same physical cell boundaries");
}
void sourceFillHandlePixels(bool range){
 Fixture f;f.view.horizontalHeader()->setDefaultSectionSize(64);f.view.verticalHeader()->setDefaultSectionSize(20);f.select(5,1,5,range?9:1);const auto before=f.session.snapshot();const auto first=f.view.visualRect(f.model.index(5,1)),last=f.view.visualRect(f.model.index(5,range?9:1));const auto image=f.view.viewport()->grab().toImage();const QRect handle(last.bottomRight()-QPoint(1,1),QSize(5,5));
 for(int y=handle.top();y<=handle.bottom();++y)for(int x=handle.left();x<=handle.right();++x)require(image.pixelColor(x,y)==QColor("#217346"),"source five-by-five fill handle sits at the selected range's bottom-right gridline intersection");
 require(image.pixelColor(handle.topLeft()-QPoint(0,1))!=QColor("#217346"),"source fill handle does not retain the old seven-pixel square above the corner");
 if(range)require(image.pixelColor(first.bottomRight()-QPoint(2,3))!=QColor("#217346"),"multi-cell selection does not leave a second handle on its active first cell");
 f.view.setShowGrid(false);QApplication::processEvents();const auto withoutGrid=f.view.viewport()->grab().toImage();for(int y=handle.top();y<=handle.bottom();++y)for(int x=handle.left();x<=handle.right();++x)require(withoutGrid.pixelColor(x,y)==QColor("#217346"),"hiding gridlines leaves the fill knob at the same physical cell corner");require(f.session.snapshot()==before,"source fill-handle painting creates no data or history");
}
void rangeHandleClickIsReadOnly(){
 Fixture f;require(f.model.setData(f.model.index(5,1),"first",Qt::EditRole)&&f.model.setData(f.model.index(5,9),"last",Qt::EditRole),"seed distinct range endpoints");f.select(5,1,5,9);const auto before=f.session.snapshot();const auto selected=f.view.selectionModel()->selection();const auto point=f.view.visualRect(f.model.index(5,9)).bottomRight()+QPoint(1,1);QTest::mouseClick(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,point);QApplication::processEvents();require(f.session.snapshot()==before&&f.view.selectionModel()->selection()==selected,"clicking the visible range fill handle without dragging preserves every cell, style, selection and history");
}

void sourceLabelsClipWithoutEllipses(){
 Fixture f;f.view.setWordWrap(false);f.view.setColumnWidth(1,150);f.view.setRowHeight(3,20);f.view.setRowHeight(4,20);require(f.model.setData(f.model.index(3,1),"Selling, general and administrative",Qt::EditRole)&&f.model.setData(f.model.index(3,2),"occupied",Qt::EditRole)&&f.model.setData(f.model.index(4,1),"123456789012345",Qt::EditRole),"seed the source raw label, occupied next cell and numeric overflow boundary");require(f.model.apply(Json::array({{{"type","set_style"},{"sheet",f.model.sheetId().toStdString()},{"row",5},{"col",2},{"style",{{"numberFormat","0.00000000000000000000"}}}}})),"seed an explicitly wide numeric format boundary");QApplication::processEvents();f.paint();const auto before=f.session.snapshot();
 for(const bool numeric:{false,true}){
  const auto index=f.model.index(numeric?4:3,1);QStyleOptionViewItem observed;bool found=false;for(const auto& cell:f.style.cells)if(cell.index==index){observed=cell;found=true;break;}require(found,"actual worksheet delegate supplies the visible source label option");const auto actual=f.view.viewport()->grab().toImage().copy(observed.rect).convertToFormat(QImage::Format_ARGB32);
  auto render=[&](Qt::TextElideMode mode){QImage result(observed.rect.size(),QImage::Format_ARGB32);result.fill(Qt::white);QPainter painter(&result);auto option=observed;option.rect.moveTopLeft(QPoint());option.textElideMode=mode;QStyledItemDelegate independent;independent.paint(&painter,option,index);painter.end();return result;};const auto clipped=render(Qt::ElideNone),elided=render(Qt::ElideRight);require(clipped!=elided,"native pixel fixture visibly distinguishes clipping from three-dot substitution");
  require(actual==(numeric?elided:clipped),numeric?"text clipping does not silently change the existing numeric overflow presentation":"source raw labels visibly clip at the cell edge instead of replacing their ending with ellipsis glyphs");
 }
 require(f.session.snapshot()==before,"clipped labels do not alter stored text, formats or history");
}
void sourceTextOverflowsOnlyEmptyNeighbors(){
 Fixture f;f.view.setWordWrap(false);f.view.setShowGrid(false);f.view.horizontalHeader()->setDefaultSectionSize(64);f.view.verticalHeader()->setDefaultSectionSize(20);f.view.setColumnWidth(0,90);f.select(0,8,0,8);const auto sheet=f.model.sheetId().toStdString();
 require(f.model.setData(f.model.index(18,6),"occupied",Qt::EditRole)&&f.model.setData(f.model.index(19,0),"Wrapped pension label must remain inside its cell",Qt::EditRole)&&f.model.setData(f.model.index(20,0),"123456789012345",Qt::EditRole)&&f.model.setData(f.model.index(21,0),"Merged pension label",Qt::EditRole)&&f.model.setData(f.model.index(22,1),"=\"\"",Qt::EditRole),"seed occupied, wrapped, numeric, merged and formula-empty overflow boundaries");
 require(f.model.apply(Json::array({{{"type","set_style"},{"sheet",sheet},{"row",20},{"col",1},{"style",{{"wrap",true}}}}})),"keep wrapped source text within its own cell");f.view.setSpan(21,0,1,2);QApplication::processEvents();const auto baseline=f.view.viewport()->grab().toImage();
 const QString label="Non-service pension cost (benefit)";require(f.model.setData(f.model.index(18,0),label,Qt::EditRole)&&f.model.setData(f.model.index(22,0),label,Qt::EditRole),"enter the observed post-unmerge pension label");QApplication::processEvents();const auto before=f.session.snapshot();f.paint();const auto actual=f.view.viewport()->grab().toImage();
 auto interior=[&](int row,int column){return f.view.visualRect(f.model.index(row,column)).adjusted(4,3,-4,-3);};auto darkPixels=[&](const QRect& rect){int count=0;for(int y=rect.top();y<=rect.bottom();++y)for(int x=rect.left();x<=rect.right();++x){const auto color=actual.pixelColor(x,y);if(color.red()<120&&color.green()<120&&color.blue()<120)++count;}return count;};
 require(darkPixels(interior(18,1))>12,"the source post-unmerge pension label visibly continues into the adjacent empty cell");require(actual.copy(interior(18,6))==baseline.copy(interior(18,6)),"text overflow stops before an occupied neighboring cell without darkening its text");
 const auto source=f.model.index(18,0);const auto full=f.view.visualRect(source).united(f.view.visualRect(f.model.index(18,5)));QStyleOptionViewItem option;bool found=false;for(const auto& cell:f.style.cells)if(cell.index==source){option=cell;found=true;break;}require(found,"record the actual source label's native font and baseline");QImage whole(full.size(),QImage::Format_ARGB32);whole.fill(Qt::white);QPainter painter(&whole);option.rect=whole.rect();option.textElideMode=Qt::ElideNone;QStyledItemDelegate independent;independent.paint(&painter,option,source);painter.end();require(actual.copy(full).convertToFormat(QImage::Format_ARGB32)==whole,"overflow draws the complete native label once across the original cell boundary, without a padding gap or doubled glyph pixels");
 for(const auto cell:{QPoint(1,19),QPoint(1,20),QPoint(2,21),QPoint(2,22)})require(darkPixels(interior(cell.y(),cell.x()))==0,"wrapped, numeric and merged cells retain their native overflow boundaries");require(actual.copy(interior(22,1))==baseline.copy(interior(22,1)),"even an empty-result formula blocks overflow and retains its native presentation");require(f.session.snapshot()==before,"text overflow is paint-only and preserves stored cells, styles and history");if(const auto path=qEnvironmentVariable("TIGERSHEETS_SOURCE_OVERFLOW_CAPTURE");!path.isEmpty())require(f.view.grab().save(path),"retain the actual source-label overflow native fixture");
}
void sourceOverflowRepaintsChangedRows(){
 Fixture f;f.view.setWordWrap(false);f.view.setShowGrid(false);f.view.horizontalHeader()->setDefaultSectionSize(64);f.view.verticalHeader()->setDefaultSectionSize(20);f.select(0,8,0,8);const auto origin=f.model.index(18,0),blocker=f.model.index(18,1);const QString label="Non-service pension cost (benefit)";require(f.model.setData(origin,label,Qt::EditRole),"seed the visible source spill away from the active cell");QApplication::processEvents();PaintObserver observer;f.view.viewport()->installEventFilter(&observer);const auto left=f.view.visualRect(origin).center(),far=f.view.visualRect(f.model.index(18,3)).center();
 auto mutation=[&](const auto& change){observer.painted=QRegion();change();require(QTest::qWaitFor([&]{return !observer.painted.isEmpty();},1000),"a narrow changed-cell signal produces an actual viewport paint event");require(observer.painted.contains(left)&&observer.painted.contains(far),"changing a source or blocker repaints the visible spill row, even when the active cell is elsewhere");};
 mutation([&]{require(f.model.setData(blocker,"blocked",Qt::EditRole),"insert an occupied spill barrier");});mutation([&]{require(f.model.setData(blocker,"",Qt::EditRole),"remove the spill barrier");});mutation([&]{require(f.model.setData(origin,"short",Qt::EditRole),"shorten the source and clear its old neighboring glyphs");});mutation([&]{require(f.model.setData(origin,"=\"label\"",Qt::EditRole),"seed a formula whose calculated text changes");});mutation([&]{f.model.setCalculated({{"revision",f.session.snapshotView().at("revision")},{"values",Json::array({{{"sheet",f.model.sheetId().toStdString()},{"row",19},{"col",1},{"value",label.toStdString()}}})}});});
 const auto before=f.session.snapshot();QApplication::processEvents();require(f.session.snapshot()==before,"spill repaint invalidation adds no data changes or history");
}

}
int main(int argc,char** argv){QApplication app(argc,argv);try{const auto selected=argc>1?std::string(argv[1]):std::string();if(selected.empty()||selected=="drag_dimensions")offscreenAnchorShowsFloatingRangeDimensions();if(selected.empty()||selected=="long_drag")draggingPastItsAnchorPreservesTheViewport();if(selected.empty()||selected=="source_overflow_update")sourceOverflowRepaintsChangedRows();if(selected.empty()||selected=="source_overflow")sourceTextOverflowsOnlyEmptyNeighbors();if(selected.empty()||selected=="source_clip")sourceLabelsClipWithoutEllipses();if(selected.empty()||selected=="source_border")sourceCellOutlinePixels();if(selected.empty()||selected=="source_handle")sourceFillHandlePixels(false);if(selected.empty()||selected=="source_range_handle")sourceFillHandlePixels(true);if(selected.empty()||selected=="source_range_click")rangeHandleClickIsReadOnly();if(selected.empty()||selected=="formatting")selectedCellsRetainFormatting();if(selected.empty()||selected=="range_border")selectedRangeHasOuterBorder();if(selected.empty()||selected=="row_header")headerDragDoesNotLeaveACellHandle(false);if(selected.empty()||selected=="column_header")headerDragDoesNotLeaveACellHandle(true);if(selected.empty()||selected=="extend")extendingSelectionRepaintsItsOldBorder();if(selected.empty()||selected=="outside_current")currentCellOutsideSelectionHasNoDecoration();if(selected.empty()||selected=="merged_border")mergedSelectionHasOneOuterBorder();if(selected.empty()||selected=="disjoint")disjointSelectionsKeepSeparateBorders();std::cout<<"worksheet selection UI tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
