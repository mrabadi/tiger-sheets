#include "tigersheets/worksheet_border_drawing.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QSet>
#include <algorithm>
namespace tigersheets {
namespace {
class BorderDrawing final:public QObject {
 WorksheetView& view_;WorksheetModel& model_;QPointer<QRubberBand> preview_;
 bool active_=false,applying_=false;QModelIndex anchor_;QString sheet_,style_,color_,mode_;QCursor previous_;
 void stop(){if(!active_)return;active_=false;anchor_={};preview_->hide();view_.viewport()->setCursor(previous_);}
 void preview(const QModelIndex& end){if(!anchor_.isValid()||!end.isValid())return;preview_->setGeometry(view_.visualRect(anchor_).united(view_.visualRect(end)));preview_->show();}
 void commit(const QModelIndex& end){
  preview_->hide();if(!anchor_.isValid()||!end.isValid()||sheet_!=model_.sheetId()){anchor_={};return;}
  const int top=std::min(anchor_.row(),end.row()),bottom=std::max(anchor_.row(),end.row()),left=std::min(anchor_.column(),end.column()),right=std::max(anchor_.column(),end.column());anchor_={};
  if(static_cast<qint64>(bottom-top+1)*(right-left+1)>100000)return;
  const Json edge={{"style",style_.toStdString()},{"color",color_.toStdString()}};Json operations=Json::array();
  for(int row=top;row<=bottom;++row)for(int col=left;col<=right;++col){
   auto border=model_.cellStyle(row,col).value("border",Json::object());bool changed=false;
   for(const auto& side:{"top","bottom","left","right"}){
    const bool boundary=mode_=="grid"||mode_=="erase"||(std::string_view(side)=="top"&&row==top)||(std::string_view(side)=="bottom"&&row==bottom)||(std::string_view(side)=="left"&&col==left)||(std::string_view(side)=="right"&&col==right);
    if(boundary){if(mode_=="erase"||style_=="none")border.erase(side);else border[side]=edge;changed=true;}
   }
   if(changed)operations.push_back({{"type","set_style"},{"sheet",sheet_.toStdString()},{"row",row+1},{"col",col+1},{"style",{{"border",border}}}});
  }
  applying_=true;model_.apply(operations);applying_=false;
 }
 bool eventFilter(QObject* object,QEvent* event)override{
  if(!active_)return false;
  if(event->type()==QEvent::KeyPress){auto* widget=qobject_cast<QWidget*>(object);if(widget&&widget->window()==view_.window()&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape){stop();return true;}}
  if(object!=view_.viewport())return false;
  if(event->type()==QEvent::MouseButtonPress){auto* mouse=static_cast<QMouseEvent*>(event);if(mouse->button()!=Qt::LeftButton)return false;anchor_=view_.indexAt(mouse->position().toPoint());sheet_=model_.sheetId();preview(anchor_);return true;}
  if(event->type()==QEvent::MouseMove&&anchor_.isValid()){preview(view_.indexAt(static_cast<QMouseEvent*>(event)->position().toPoint()));return true;}
  if(event->type()==QEvent::MouseButtonRelease&&anchor_.isValid()){auto* mouse=static_cast<QMouseEvent*>(event);if(mouse->button()==Qt::LeftButton){commit(view_.indexAt(mouse->position().toPoint()));return true;}}
  return false;
 }
public:
 BorderDrawing(WorksheetView& view,WorksheetModel& model):QObject(&view),view_(view),model_(model),preview_(new QRubberBand(QRubberBand::Rectangle,view.viewport())){
  setObjectName("worksheet.borderDrawing");preview_->setAttribute(Qt::WA_TransparentForMouseEvents);qApp->installEventFilter(this);
  connect(&model_,&QAbstractItemModel::modelAboutToBeReset,this,[this]{if(!applying_)stop();});
 }
 ~BorderDrawing()override{qApp->removeEventFilter(this);}
 bool begin(const QString& mode,const QString& style,const QString& color){
  static const QSet<QString> styles{"none","thin","medium","dashed","dotted","thick","double","hair","mediumDashed","dashDot","mediumDashDot","dashDotDot","mediumDashDotDot","slantDashDot"};
  if((mode!="draw"&&mode!="grid"&&mode!="erase")||!styles.contains(style)||!QColor(color).isValid())return false;
  stop();previous_=view_.viewport()->cursor();mode_=mode;style_=style;color_=QColor(color).name();active_=true;
  QPixmap pixels(24,24);pixels.fill(Qt::transparent);QPainter painter(&pixels);painter.setPen(QPen(Qt::black,1));if(mode=="erase"){painter.setBrush(QColor("#c4a0cc"));painter.drawPolygon(QPolygon({{3,16},{15,4},{23,12},{11,24}}));painter.setBrush(Qt::white);painter.drawPolygon(QPolygon({{3,16},{7,12},{15,20},{11,24}}));}else{painter.setBrush(QColor("#f3c35b"));painter.drawPolygon(QPolygon({{3,21},{5,13},{17,1},{23,7},{11,19}}));painter.setBrush(Qt::black);painter.drawPolygon(QPolygon({{3,21},{5,15},{9,19}}));}painter.end();view_.viewport()->setCursor(QCursor(pixels,3,21));view_.setFocus();return true;
 }
};
}
bool beginWorksheetBorderDrawing(WorksheetView& view,WorksheetModel& model,const QString& mode,const QString& style,const QString& color){
 auto* drawing=dynamic_cast<BorderDrawing*>(view.findChild<QObject*>("worksheet.borderDrawing"));if(!drawing)drawing=new BorderDrawing(view,model);return drawing->begin(mode,style,color);
}
}
