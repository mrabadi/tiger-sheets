#include "tigersheets/worksheet_sheet_bar.hpp"
#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTabBar>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
namespace tigersheets {
namespace {
QIcon sheetIcon(int direction){
 QIcon icon;for(bool enabled:{true,false}){QPixmap image(20,20);image.fill(Qt::transparent);QPainter painter(&image);const QColor color(enabled?"#7f7f7f":"#c4c4c4");painter.setPen(QPen(color,1.2));if(direction){painter.setBrush(color);if(direction<0)painter.drawPolygon(QPolygon{{8,10},{12,6},{12,14}});else painter.drawPolygon(QPolygon{{12,10},{8,6},{8,14}});}else{painter.setRenderHint(QPainter::Antialiasing);painter.drawEllipse(QRectF(3,3,14,14));painter.drawLine(6,10,14,10);painter.drawLine(10,6,10,14);}painter.end();icon.addPixmap(image,enabled?QIcon::Normal:QIcon::Disabled);}return icon;
}
class WorksheetSheetBar final:public QWidget {
 QTableView& grid_;QTabBar& tabs_;QScrollBar* scroll_;QToolButton* previous_;QToolButton* next_;QToolButton* add_;QWidget* divider_;double split_=786.0/1280.0;int dragStart_=0;bool dragging_=false;
 QToolButton* nativeTabScroll(Qt::ArrowType direction)const{for(auto* button:tabs_.findChildren<QToolButton*>())if(button->arrowType()==direction)return button;return nullptr;}
 void synchronize(){
  const int split=std::clamp(qRound(width()*split_),std::min(300,width()/2),std::max(300,width()-100));
  const int available=std::max(0,split-6-103-56);const int tabWidth=std::min(available,tabs_.sizeHint().width());tabs_.setGeometry(103,0,tabWidth,28);previous_->setGeometry(12,0,32,28);next_->setGeometry(44,0,32,28);add_->setGeometry(103+tabWidth+29,2,24,24);scroll_->setGeometry(split,7,std::max(0,width()-22-split),15);divider_->setGeometry(split-6,0,6,28);
  for(auto entry:{std::pair{previous_,Qt::LeftArrow},std::pair{next_,Qt::RightArrow}}){auto* native=nativeTabScroll(entry.second);entry.first->setEnabled(native&&native->isVisible()&&native->isEnabled());}
 }
 bool eventFilter(QObject* object,QEvent* event)override{
  if(object==divider_){if(event->type()==QEvent::MouseButtonPress){auto* mouse=static_cast<QMouseEvent*>(event);if(mouse->button()==Qt::LeftButton){dragging_=true;dragStart_=qRound(mouse->globalPosition().x());return true;}}else if(event->type()==QEvent::MouseMove&&dragging_){auto* mouse=static_cast<QMouseEvent*>(event);const int current=qRound(mouse->globalPosition().x());split_=std::clamp(split_+double(current-dragStart_)/std::max(1,width()),0.25,0.85);dragStart_=current;synchronize();return true;}else if(event->type()==QEvent::MouseButtonRelease){dragging_=false;return true;}}
  if(object==&tabs_&&(event->type()==QEvent::LayoutRequest||event->type()==QEvent::Resize||event->type()==QEvent::Show))QTimer::singleShot(0,this,[this]{synchronize();});return QWidget::eventFilter(object,event);
 }
 void resizeEvent(QResizeEvent* event)override{QWidget::resizeEvent(event);synchronize();}
public:
 WorksheetSheetBar(MainWindow& window,QTableView& grid,QTabBar& tabs):QWidget(&window),grid_(grid),tabs_(tabs),scroll_(grid.horizontalScrollBar()){
  setObjectName("workspace.sheetBar");setFixedHeight(28);setStyleSheet("QWidget#workspace\\.sheetBar { background:#e7e7e7; border-top:1px solid #a0a0a0; } QToolButton { border:0; background:transparent; padding:0; }");tabs_.setParent(this);tabs_.setFont(QFont("Calibri",11));tabs_.setFixedHeight(28);tabs_.setStyleSheet("QTabBar::tab { height:26px; padding:0px 12px; }");tabs_.installEventFilter(this);
  grid_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);grid_.verticalScrollBar()->setStyleSheet("QScrollBar:vertical { width:22px; }");grid_.verticalScrollBar()->setFixedWidth(22);scroll_->setParent(this);scroll_->setObjectName("worksheet.horizontalScrollBar");scroll_->show();
  auto button=[&](const char* name,const char* label,int direction){auto* value=new QToolButton(this);value->setObjectName(name);value->setAccessibleName(label);value->setToolTip(label);value->setIcon(sheetIcon(direction));value->setIconSize(QSize(20,20));value->setAutoRaise(true);return value;};previous_=button("worksheet.previousSheetTabs","Scroll sheet tabs left",-1);next_=button("worksheet.nextSheetTabs","Scroll sheet tabs right",1);add_=button("worksheet.addSheet","New sheet",0);connect(add_,&QToolButton::clicked,&window,[&window]{window.executeCommand("sheet.add");});for(auto entry:{std::pair{previous_,Qt::LeftArrow},std::pair{next_,Qt::RightArrow}})connect(entry.first,&QToolButton::clicked,this,[this,direction=entry.second]{if(auto* native=nativeTabScroll(direction))native->click();synchronize();});
  divider_=new QWidget(this);divider_->setObjectName("worksheet.sheetScrollDivider");divider_->setCursor(Qt::SplitHCursor);divider_->setStyleSheet("background:#e7e7e7; border-left:1px solid #a5a5a5; border-right:1px solid white;");divider_->installEventFilter(this);connect(&tabs_,&QTabBar::currentChanged,this,[this]{QTimer::singleShot(0,this,[this]{synchronize();});});QTimer::singleShot(0,this,[this]{synchronize();});
 }
};
}
QWidget* createWorksheetSheetBar(MainWindow& window,QTableView& grid,QTabBar& tabs){return new WorksheetSheetBar(window,grid,tabs);}
}
