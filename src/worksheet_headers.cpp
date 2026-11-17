#include "tigersheets/worksheet_headers.hpp"
#include <QAbstractButton>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPointer>
#include <QTableView>
#include <algorithm>
namespace tigersheets {
namespace {
class HeaderPainter final:public QObject {
 QTableView& view_;
 QPointer<QAbstractButton> corner_;
 QMetaObject::Connection selection_,current_;
 int selectionState(Qt::Orientation orientation,int section)const{
  if(view_.property("worksheet.externalPointing").toBool())return 0;
  if(!view_.selectionModel()||!view_.model())return 0;int state=0;
  for(const auto& range:view_.selectionModel()->selection())if(orientation==Qt::Horizontal?(range.left()<=section&&section<=range.right()):(range.top()<=section&&section<=range.bottom())){state=1;if(orientation==Qt::Horizontal?(range.top()==0&&range.bottom()==view_.model()->rowCount()-1):(range.left()==0&&range.right()==view_.model()->columnCount()-1))return 2;}
  return state;
 }
 bool allSelected()const{if(!view_.selectionModel()||!view_.model())return false;for(const auto& range:view_.selectionModel()->selection())if(range.top()==0&&range.left()==0&&range.bottom()==view_.model()->rowCount()-1&&range.right()==view_.model()->columnCount()-1)return true;return false;}
 void update(){view_.horizontalHeader()->viewport()->update();view_.verticalHeader()->viewport()->update();if(corner_)corner_->update();}
 void paintHeader(QHeaderView& header){
  auto* surface=header.viewport();QPainter painter(surface);painter.fillRect(surface->rect(),QColor("#e7e7e7"));if(!view_.model())return;
  QFont font=header.font();font.setBold(false);painter.setFont(font);const bool horizontal=header.orientation()==Qt::Horizontal;const int length=horizontal?surface->width():surface->height();
  for(int visual=std::max(0,header.visualIndexAt(0));visual<header.count();++visual){
   const int section=header.logicalIndex(visual);if(header.isSectionHidden(section))continue;const int position=header.sectionViewportPosition(section),size=header.sectionSize(section);if(position>=length)break;if(position+size<=0)continue;
   const QRect rectangle=horizontal?QRect(position,0,size,surface->height()):QRect(0,position,surface->width(),size);const int active=selectionState(header.orientation(),section);painter.fillRect(rectangle,QColor(active==2?"#d3f0e0":active?"#d2d2d2":"#e7e7e7"));painter.setPen(QColor(active==2?"#b7d4c6":"#c1c1c1"));if(horizontal)painter.drawLine(rectangle.topRight(),rectangle.bottomRight());else painter.drawLine(rectangle.bottomLeft(),rectangle.bottomRight());painter.setPen(QColor("#9d9d9d"));if(horizontal)painter.drawLine(rectangle.bottomLeft(),rectangle.bottomRight());else painter.drawLine(rectangle.topRight(),rectangle.bottomRight());
   painter.setPen(QColor(active==2?"#217346":"#444444"));painter.drawText(horizontal?rectangle:rectangle.translated(2,0),Qt::AlignHCenter|Qt::AlignVCenter,view_.model()->headerData(section,header.orientation(),Qt::DisplayRole).toString());
   if(active)painter.fillRect(horizontal?QRect(rectangle.left(),rectangle.bottom()-1,rectangle.width(),2):QRect(rectangle.right()-1,rectangle.top(),2,rectangle.height()),QColor("#217346"));
  }
 }
 bool eventFilter(QObject* watched,QEvent* event)override{
  if(watched==&view_&&event->type()==QEvent::DynamicPropertyChange){update();return false;}
  if(event->type()!=QEvent::Paint||view_.layoutDirection()!=Qt::LeftToRight)return false;
  if(watched==view_.horizontalHeader()->viewport()){paintHeader(*view_.horizontalHeader());return true;}
  if(watched==view_.verticalHeader()->viewport()){paintHeader(*view_.verticalHeader());return true;}
  if(watched==corner_){QPainter painter(corner_);const bool active=allSelected();painter.fillRect(corner_->rect(),QColor(active?"#d3f0e0":"#e7e7e7"));painter.setPen(Qt::NoPen);painter.setBrush(QColor(active?"#217346":"#b0b0b0"));const int right=corner_->width()-2,bottom=corner_->height()-6;painter.drawPolygon(QPolygon{{right-11,bottom},{right,2},{right,bottom}});return true;}
  return false;
 }
public:
 explicit HeaderPainter(QTableView& view):QObject(&view),view_(view){setObjectName("worksheet.headersController");view.installEventFilter(this);view.horizontalHeader()->viewport()->installEventFilter(this);view.verticalHeader()->viewport()->installEventFilter(this);for(auto* button:view.findChildren<QAbstractButton*>())if(QByteArray(button->metaObject()->className())=="QTableCornerButton"){corner_=button;button->installEventFilter(this);break;}}
 void bind(){QObject::disconnect(selection_);QObject::disconnect(current_);if(auto* model=view_.selectionModel()){selection_=connect(model,&QItemSelectionModel::selectionChanged,this,[this]{update();});current_=connect(model,&QItemSelectionModel::currentChanged,this,[this]{update();});}update();}
};
}
void installWorksheetHeaders(QTableView& view){auto* painter=dynamic_cast<HeaderPainter*>(view.findChild<QObject*>("worksheet.headersController",Qt::FindDirectChildrenOnly));if(!painter)painter=new HeaderPainter(view);painter->bind();}
}
