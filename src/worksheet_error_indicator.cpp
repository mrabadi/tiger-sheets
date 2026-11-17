#include "tigersheets/worksheet_error_indicator.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
namespace tigersheets {
namespace {
QIcon warningIcon(){
 QPixmap image(16,16);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor("#947640"),1));painter.setBrush(QColor("#ffe9a1"));painter.drawPolygon(QPolygonF{{8,1},{15,14},{1,14}});painter.setPen(QPen(QColor("#363636"),1.8,Qt::SolidLine,Qt::RoundCap));painter.drawLine(QPointF(8,5),QPointF(8,9));painter.drawPoint(QPointF(8,11.5));return QIcon(image);
}
class ErrorIndicator final:public QObject {
 QPointer<MainWindow> window_;QPointer<WorksheetModel> model_;QPointer<WorksheetView> view_;QPointer<QToolButton> button_;QPointer<QMenu> menu_;
 void schedule(){QTimer::singleShot(0,this,[this]{refresh();});}
 void refresh(){
  if(!window_||!model_||!view_||!button_)return;
  const auto index=view_->currentIndex();const auto mode=view_->property("worksheet.entryMode").toString();
  if(!index.isValid()||(!window_->isActiveWindow()&&!(menu_&&menu_->isVisible()))||view_->activeCellEditor()||(!mode.isEmpty()&&mode!="Ready")||view_->property("worksheet.externalPointing").toBool()||!model_->data(index,FormulaErrorCheckRole).toBool()){button_->hide();return;}
  const auto rect=view_->visualRect(index);if(!rect.intersects(view_->viewport()->rect())||rect.top()<0){button_->hide();return;}
  const auto boundary=rect.topLeft()-QPoint(1,1);int left=boundary.x()-33;if(left<0)left=rect.right()+2;left=std::clamp(left,0,std::max(0,view_->viewport()->width()-button_->width()));button_->move(left,boundary.y()+1);button_->setVisible(button_->geometry().bottom()<view_->viewport()->height());button_->raise();
 }
 void open(){
  if(!window_||!model_||!view_)return;const QPersistentModelIndex target=view_->currentIndex();if(!target.isValid()||!model_->data(target,FormulaErrorCheckRole).toBool())return;
  delete menu_.data();auto* menu=new QMenu(window_);menu_=menu;menu->setObjectName("worksheet.errorMenu");const bool numericText=model_->raw(target.row(),target.column()).startsWith('\'');
  auto* reason=menu->addAction(numericText?"Number Stored as Text":"Formula Omits Adjacent Cells");reason->setEnabled(false);
  auto* edit=menu->addAction("Edit in Formula Bar");edit->setObjectName("worksheet.error.edit");connect(edit,&QAction::triggered,this,[this,target]{if(!window_||!view_||!target.isValid())return;view_->setCurrentIndex(target);if(auto* formula=window_->findChild<QLineEdit*>("formula.input")){formula->setFocus();formula->setCursorPosition(formula->text().size());}});
  menu->popup(button_->mapToGlobal(QPoint(0,button_->height())));
 }
 bool eventFilter(QObject* object,QEvent* event)override{
  if(!view_||!button_)return false;
  if(object==window_&&(event->type()==QEvent::WindowActivate||event->type()==QEvent::WindowDeactivate||event->type()==QEvent::Hide)){if(event->type()!=QEvent::WindowActivate)button_->hide();schedule();}
  if((object==view_&&event->type()==QEvent::DynamicPropertyChange)||(object==view_->viewport()&&event->type()==QEvent::Resize))schedule();
  return false;
 }
public:
 ErrorIndicator(MainWindow& window,WorksheetModel& model,WorksheetView& view):QObject(&window),window_(&window),model_(&model),view_(&view){
  setObjectName("worksheet.errorIndicatorController");auto* button=new QToolButton(view.viewport());button_=button;button->setObjectName("worksheet.errorIndicator");button->setAccessibleName("Error Checking");button->setToolTip("Error Checking");button->setIcon(warningIcon());button->setIconSize(QSize(16,16));button->setFixedSize(19,20);button->setFocusPolicy(Qt::NoFocus);button->setStyleSheet("QToolButton { padding: 0px; border: 1px solid #b8b8b8; border-radius: 0px; background: white; } QToolButton:hover { background: #fff4ce; }");button->hide();connect(button,&QToolButton::clicked,this,[this]{open();});
  connect(view.selectionModel(),&QItemSelectionModel::currentChanged,this,[this]{refresh();});connect(view.selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{refresh();});connect(&model,&QAbstractItemModel::dataChanged,this,[this]{refresh();});connect(&model,&QAbstractItemModel::modelReset,this,[this]{schedule();});connect(view.horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});connect(view.verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});connect(view.horizontalHeader(),&QHeaderView::sectionResized,this,[this]{refresh();});connect(view.verticalHeader(),&QHeaderView::sectionResized,this,[this]{refresh();});connect(qApp,&QApplication::focusChanged,this,[this]{schedule();});qApp->installEventFilter(this);refresh();
 }
 ~ErrorIndicator()override{qApp->removeEventFilter(this);delete menu_.data();delete button_.data();}
};
}
QObject* installWorksheetErrorIndicator(MainWindow& window,WorksheetModel& model,WorksheetView& view){return new ErrorIndicator(window,model,view);}
}
