#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPicture>
#include <QProxyStyle>
#include <QStyleOptionViewItem>
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
bool borderAt(const std::vector<PaintedRect>& paint,const QRect& rect){for(const auto& draw:paint)if(draw.pen.style()!=Qt::NoPen&&draw.pen.color()==QColor("#245648")&&draw.pen.widthF()==2&&draw.brush.style()==Qt::NoBrush&&draw.rect==QRectF(rect))return true;return false;}
bool hasFillHandle(const std::vector<PaintedRect>& paint){for(const auto& draw:paint)if(draw.pen.style()==Qt::NoPen&&draw.brush.color()==QColor("#245648")&&draw.rect.size()==QSizeF(7,7))return true;return false;}
void dragHeader(QHeaderView* header,int first,int last){const bool horizontal=header->orientation()==Qt::Horizontal;auto center=[&](int section){const int along=header->sectionViewportPosition(section)+header->sectionSize(section)/2;return horizontal?QPoint(along,header->viewport()->height()/2):QPoint(header->viewport()->width()/2,along);};const auto start=center(first),end=center(last);QTest::mousePress(header->viewport(),Qt::LeftButton,Qt::NoModifier,start);QMouseEvent move(QEvent::MouseMove,end,header->viewport()->mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(header->viewport(),&move);QTest::mouseRelease(header->viewport(),Qt::LeftButton,Qt::NoModifier,end);QApplication::processEvents();}
void selectedCellsRetainFormatting(){
 Fixture f;const auto sheet=f.model.sheetId().toStdString();require(f.model.apply(Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",2},{"col",2},{"input","Formatted"},{"style",{{"fill","#1478be"},{"color","#ffffff"},{"bold",true},{"italic",true},{"numberFormat","0.00"}}}}})),"seed cell colors and font");const auto before=f.session.snapshot();f.select(1,0,2,3);f.paint();const auto target=f.model.index(1,1);bool found=false;
 for(const auto& cell:f.style.cells)if(cell.index==target){found=true;const bool selected=cell.state&QStyle::State_Selected;const auto background=selected?cell.palette.color(QPalette::Highlight):cell.backgroundBrush.color();const auto foreground=cell.palette.color(selected?QPalette::HighlightedText:QPalette::Text);require(background.blue()>background.red()*2&&background.blue()>100,"selection tint retains the cell's blue fill instead of replacing it with a flat selection color");require(foreground==QColor("#ffffff"),"selected text retains its explicit white font color");require(cell.font.bold()&&cell.font.italic(),"selection retains font formatting");}
 require(found,"native cell delegate paints the selected formatted cell");require(f.session.snapshot()==before,"selecting cells never mutates their stored formatting or history");
}
void selectedRangeHasOuterBorder(){
 Fixture f;f.select(1,1,1,1);const auto active=f.view.visualRect(f.model.index(1,1)).adjusted(1,1,-1,-1);require(borderAt(f.paint(),active),"vector paint recorder observes the existing single-cell border");f.select(1,1,3,3);const auto paint=f.paint();const auto bounds=f.view.visualRect(f.model.index(1,1)).united(f.view.visualRect(f.model.index(3,3))).adjusted(1,1,-1,-1);require(borderAt(paint,bounds),"a rectangular selection paints one outer border around its full range");require(!borderAt(paint,active),"a multi-cell selection does not leave a separate active-cell border inside it");
}
void headerDragDoesNotLeaveACellHandle(bool columns){
 Fixture f;f.view.setCurrentIndex(f.model.index(12,6));const auto before=f.session.snapshot();dragHeader(columns?f.view.horizontalHeader():f.view.verticalHeader(),1,3);const auto selection=f.view.selectionModel()->selection();require(selection.size()==1,"header drag creates one contiguous selection rather than stray cells");const auto& range=selection.front();require(columns?(range.left()==1&&range.right()==3&&range.top()==0&&range.bottom()==f.model.rowCount()-1):(range.top()==1&&range.bottom()==3&&range.left()==0&&range.right()==f.model.columnCount()-1),"native header drag selects exactly the intended full columns or rows");require(!hasFillHandle(f.paint()),"selecting whole rows or columns does not paint a stray single-cell fill handle");require(f.session.snapshot()==before,"header drag preserves workbook data, formatting and history");
}
class PaintObserver final:public QObject {
public:QRegion painted;
protected:bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::Paint)painted+=static_cast<QPaintEvent*>(event)->region();return false;}
};
void extendingSelectionRepaintsItsOldBorder(){
 Fixture f;f.select(1,1,3,3);QApplication::processEvents();const auto bounds=f.view.visualRect(f.model.index(1,1)).united(f.view.visualRect(f.model.index(3,3))).adjusted(1,1,-1,-1);PaintObserver observer;f.view.viewport()->installEventFilter(&observer);f.view.selectionModel()->select(QItemSelection(f.model.index(1,1),f.model.index(4,3)),QItemSelectionModel::ClearAndSelect);QApplication::processEvents();require(observer.painted.contains(QPoint(bounds.left()+20,bounds.bottom())),"extending a selection repaints its previous bottom edge so no interior border remains");
}
void currentCellOutsideSelectionHasNoDecoration(){
 Fixture f;f.select(1,1,3,3);const auto outside=f.model.index(12,6);f.view.selectionModel()->setCurrentIndex(outside,QItemSelectionModel::NoUpdate);require(!f.view.selectionModel()->isSelected(outside),"outside current cell is not a selected cell");const auto paint=f.paint();require(!borderAt(paint,f.view.visualRect(outside).adjusted(1,1,-1,-1))&&!hasFillHandle(paint),"a current cell outside the selection has no border or fill handle");for(const auto& cell:f.style.cells)if(cell.index==outside)require(!(cell.state&QStyle::State_HasFocus),"native delegate does not request a stray focus frame outside the selected range");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const auto selected=argc>1?std::string(argv[1]):std::string();if(selected.empty()||selected=="formatting")selectedCellsRetainFormatting();if(selected.empty()||selected=="range_border")selectedRangeHasOuterBorder();if(selected.empty()||selected=="row_header")headerDragDoesNotLeaveACellHandle(false);if(selected.empty()||selected=="column_header")headerDragDoesNotLeaveACellHandle(true);if(selected.empty()||selected=="extend")extendingSelectionRepaintsItsOldBorder();if(selected.empty()||selected=="outside_current")currentCellOutsideSelectionHasNoDecoration();std::cout<<"worksheet selection UI tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
