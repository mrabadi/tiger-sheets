#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/command_icons.hpp"
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QLabel>
#include <QMap>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <QEvent>
#include <QPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <algorithm>

namespace tigersheets {
namespace {
// QTabWidget continues laying out its stack in local coordinates after the
// stack becomes a popup window. Re-anchor after those layout requests settle.
class RibbonPopupAnchor final:public QObject {
 QPointer<QTabWidget> ribbon_;QPointer<QStackedWidget> stack_;bool pending_=false,moving_=false;
 void schedule(){if(pending_)return;pending_=true;QTimer::singleShot(0,this,[this]{pending_=false;position();});}
 bool eventFilter(QObject*,QEvent* event)override{if(!moving_&&(event->type()==QEvent::Move||event->type()==QEvent::Resize||event->type()==QEvent::Show||event->type()==QEvent::LayoutRequest))schedule();return false;}
public:
 RibbonPopupAnchor(QTabWidget& ribbon,QStackedWidget& stack):QObject(&stack),ribbon_(&ribbon),stack_(&stack){setObjectName("ribbon.popupAnchor");stack.installEventFilter(this);ribbon.installEventFilter(this);ribbon.tabBar()->installEventFilter(this);ribbon.window()->installEventFilter(this);}
 void position(){
  if(!ribbon_||!stack_||!stack_->isVisible()||stack_->windowType()!=Qt::Popup)return;
  auto target=ribbon_->tabBar()->mapToGlobal(QPoint(0,ribbon_->tabBar()->height()));
  if(const auto* page=ribbon_->currentWidget())target.ry()+=page->property("ribbon.popupTopMargin").toInt();
  // Position first: resizing publishes the new scroll range, whose keytips
  // must use the popup's settled global coordinates.
  moving_=true;
  if(stack_->mapToGlobal(QPoint())!=target)stack_->move(target);
  if(stack_->width()!=ribbon_->width())stack_->setFixedWidth(ribbon_->width());
  moving_=false;
 }
 void synchronize(){position();schedule();}
};
}
void anchorRibbonPopup(QTabWidget& ribbon,QStackedWidget& stack){auto* guard=dynamic_cast<RibbonPopupAnchor*>(stack.findChild<QObject*>("ribbon.popupAnchor"));if(!guard)guard=new RibbonPopupAnchor(ribbon,stack);guard->synchronize();}
RibbonPage::RibbonPage(QWidget* parent):QScrollArea(parent){
 setFrameShape(QFrame::NoFrame);
 setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 setWidgetResizable(true);
 setAlignment(Qt::AlignLeft|Qt::AlignTop);
 setFocusPolicy(Qt::NoFocus);
 auto* contents=new QWidget;
 contentLayout_=new QHBoxLayout(contents);
 contentLayout_->setContentsMargins(8,3,8,3);
 contentLayout_->setSpacing(5);
 // A group's minimum size may permit labels and combo boxes to collapse. Its
 // preferred size is the usable ribbon width; scroll instead of compressing it.
 contentLayout_->setSizeConstraint(QLayout::SetFixedSize);
 setWidget(contents);
 auto arrow=[&](const char* name,const char* tooltip,Qt::ArrowType type){auto* button=new QToolButton(this);button->setObjectName(name);button->setAccessibleName(tooltip);button->setToolTip(tooltip);button->setArrowType(type);button->setIcon(spreadsheetCommandIcon(type==Qt::LeftArrow?"Left":"Right"));button->setToolButtonStyle(Qt::ToolButtonIconOnly);button->setAutoRaise(true);button->setAutoRepeat(true);button->setFixedSize(24,28);button->hide();return button;};
 previous_=arrow("ribbon.scrollPrevious","Previous ribbon commands",Qt::LeftArrow);next_=arrow("ribbon.scrollNext","More ribbon commands",Qt::RightArrow);
 connect(previous_,&QToolButton::clicked,this,[this]{horizontalScrollBar()->setValue(horizontalScrollBar()->value()-std::max(100,viewport()->width()*3/4));});
 connect(next_,&QToolButton::clicked,this,[this]{horizontalScrollBar()->setValue(horizontalScrollBar()->value()+std::max(100,viewport()->width()*3/4));});
 connect(horizontalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){previous_->setEnabled(value>horizontalScrollBar()->minimum());next_->setEnabled(value<horizontalScrollBar()->maximum());});
 connect(horizontalScrollBar(),&QScrollBar::rangeChanged,this,[this]{updateOverflow();});
}
QHBoxLayout* RibbonPage::contentLayout() const noexcept{return contentLayout_;}
void RibbonPage::compactControls(){
 // Reuse the command objects so signal connections, menus, focus, and keyboard
 // commands survive changes to the ribbon's visual arrangement.
 setProperty("ribbon.contentHeight",36);setProperty("ribbon.pageHeight",68);
 contentLayout_->setContentsMargins(4,2,4,2);contentLayout_->setSpacing(0);
 for(int index=0;index<contentLayout_->count();++index){
  auto* panel=contentLayout_->itemAt(index)->widget();if(!panel)continue;
  QList<QWidget*> controls;
  std::function<void(QWidget*)> collect=[&](QWidget* parent){for(auto* child:parent->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly)){
   if(qobject_cast<QAbstractButton*>(child)||qobject_cast<QComboBox*>(child)||qobject_cast<QAbstractSpinBox*>(child))controls.append(child);else collect(child);
  }};collect(panel);
  for(auto* control:controls)control->setParent(panel);
  delete panel->layout();for(auto* child:panel->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))if(!controls.contains(child))delete child;
  panel->setMinimumSize(0,0);panel->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);panel->setFixedHeight(32);
  auto* row=new QHBoxLayout(panel);row->setContentsMargins(3,2,4,2);row->setSpacing(1);
  for(auto* control:controls){
   control->setProperty("ribbon.keyTipPoint",QVariant());
   control->setMinimumSize(0,0);control->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
   if(auto* button=qobject_cast<QAbstractButton*>(control)){
    QString name=button->accessibleName();if(name.isEmpty())name=button->text();name.replace('\n',' ');
    static const QMap<QString,QString> names={{"B","Bold"},{"I","Italic"},{"U","Underline"},{"A","Font Color"},{"%","Percent Style"},{"Σ","AutoSum"},{"▾","Paste Special"},{"Fill","Fill Color"},{"Fill ↓","Fill Down"}};
    name=names.value(name,name);button->setAccessibleName(name);button->setToolTip(name);
    if(auto* tool=qobject_cast<QToolButton*>(button)){tool->setStyleSheet(tool->styleSheet()+" QToolButton {min-height:28px;max-height:28px;min-width:28px;padding:0;}");tool->setToolButtonStyle(Qt::ToolButtonIconOnly);tool->setFixedSize(tool->popupMode()==QToolButton::MenuButtonPopup?34:28,28);tool->setIconSize(QSize(20,20));}
    if(auto* box=qobject_cast<QCheckBox*>(button)){box->setText({});box->setIcon(spreadsheetCommandIcon(box->objectName().section('.',-1)));box->setIconSize(QSize(18,18));box->setFixedSize(38,28);}
   }else{
    control->setFixedHeight(26);control->setToolTip(control->accessibleName());
    if(qobject_cast<QFontComboBox*>(control))control->setFixedWidth(124);
    else if(qobject_cast<QAbstractSpinBox*>(control))control->setFixedWidth(52);
    else if(qobject_cast<QComboBox*>(control))control->setFixedWidth(112);
   }
   row->addWidget(control,0,Qt::AlignVCenter);control->show();
  }
  row->activate();
 }
 contentLayout_->activate();updateGeometry();updateOverflow();
}
bool RibbonPage::hasOverflow() const noexcept{return overflow_;}
int RibbonPage::overflowHeight() const{return 0;}
QSize RibbonPage::sizeHint() const{return {0,widget()->sizeHint().height()+overflowHeight()};}
QSize RibbonPage::minimumSizeHint() const{return {0,0};}
void RibbonPage::resizeEvent(QResizeEvent* event){QScrollArea::resizeEvent(event);previous_->move(0,std::max(0,(height()-28)/2));next_->move(width()-24,std::max(0,(height()-28)/2));updateOverflow();}
void RibbonPage::wheelEvent(QWheelEvent* event){if(hasOverflow()){const auto delta=event->pixelDelta().isNull()?event->angleDelta()/2:event->pixelDelta();horizontalScrollBar()->setValue(horizontalScrollBar()->value()-(delta.x()?delta.x():delta.y()));event->accept();}else QScrollArea::wheelEvent(event);}
void RibbonPage::updateOverflow(){const bool next=widget()->width()>width()-2*frameWidth();previous_->setVisible(next);next_->setVisible(next);previous_->raise();next_->raise();previous_->setEnabled(horizontalScrollBar()->value()>horizontalScrollBar()->minimum());next_->setEnabled(horizontalScrollBar()->value()<horizontalScrollBar()->maximum());if(next==overflow_)return;overflow_=next;setViewportMargins(next?24:0,0,next?24:0,0);if(overflowChanged)overflowChanged(overflow_);}
}
