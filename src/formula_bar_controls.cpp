#include "tigersheets/formula_bar_controls.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/external_reference_entry.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QTabBar>
#include <QToolButton>
namespace tigersheets {
namespace {
QIcon actionIcon(bool accept){QIcon icon;for(bool enabled:{true,false}){QPixmap image(20,20);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor(enabled?"#707070":"#d0d0d0"),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));if(accept)painter.drawPolyline(QPolygonF{{4,10},{8,14},{16,5}});else{painter.drawLine(5,5,15,15);painter.drawLine(15,5,5,15);}painter.end();icon.addPixmap(image,enabled?QIcon::Normal:QIcon::Disabled);}return icon;}
QIcon nameListIcon(){QPixmap image(16,16);image.fill(Qt::transparent);QPainter painter(&image);painter.setPen(Qt::NoPen);painter.setBrush(QColor("#808080"));painter.drawPolygon(QPolygon{{5,6},{11,6},{8,9}});return QIcon(image);}
class FormulaBarControls final:public QObject {
 QPointer<MainWindow> window_;QPointer<WorksheetView> view_;QPointer<QLineEdit> bar_;QPointer<QToolButton> cancel_,accept_;bool pending_=false;
 void refresh(){if(!view_||!bar_||!cancel_||!accept_)return;const bool draft=bar_->isModified()||view_->activeCellEditor()||view_->property("worksheet.externalPointing").toBool();cancel_->setEnabled(draft);accept_->setEnabled(draft);}
 void schedule(){if(pending_)return;pending_=true;QTimer::singleShot(0,this,[this]{pending_=false;refresh();});}
 void installNameList(QLineEdit& name){
  auto* arrow=new QToolButton(&name);arrow->setObjectName("formula.nameList");arrow->setAccessibleName("Name Box list");arrow->setToolTip("Name Box");arrow->setIcon(nameListIcon());arrow->setIconSize(QSize(16,16));arrow->setFocusPolicy(Qt::NoFocus);arrow->setGeometry(84,1,17,24);arrow->setStyleSheet("QToolButton { border:0; padding:0; background:white; } QToolButton:hover { background:#eeeeee; }");name.setTextMargins(0,0,17,0);auto* menu=new QMenu(&name);menu->setObjectName("formula.nameList.menu");menu->setMinimumWidth(name.width());connect(arrow,&QToolButton::clicked,this,[this,menu,arrow]{
   if(!window_||!view_)return;menu->clear();const auto& book=window_->currentSession().snapshotView();const auto names=book.value("names",Json::object());for(const auto& [label,definition]:names.items()){
    if(!definition.is_string())continue;auto expression=definition.get<std::string>();if(expression.empty()||expression.front()!='=')expression.insert(expression.begin(),'=');const auto references=formulaReferences(expression);if(references.size()!=1||references.front().external||references.front().start!=1||references.front().length!=expression.size()-1)continue;const auto reference=references.front();auto* action=menu->addAction(QString::fromStdString(label));connect(action,&QAction::triggered,this,[this,reference]{
     if(!window_||!view_||!view_->commitPendingEditor())return;if(!reference.sheet.empty()){auto* tabs=window_->findChild<QTabBar*>("workspace.sheets");if(!tabs)return;int found=-1;for(int index=0;index<tabs->count();++index)if(tabs->tabText(index).compare(QString::fromStdString(reference.sheet),Qt::CaseInsensitive)==0){found=index;break;}if(found<0)return;tabs->setCurrentIndex(found);}auto* model=view_->model();const auto first=model->index(reference.row1-1,reference.col1-1),last=model->index(reference.row2-1,reference.col2-1);if(!first.isValid()||!last.isValid())return;view_->setCurrentIndex(first);view_->selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);view_->scrollTo(first);view_->setFocus();
    });
   }if(!menu->isEmpty())menu->popup(arrow->parentWidget()->mapToGlobal(QPoint(0,arrow->parentWidget()->height())));
  });
 }
 bool eventFilter(QObject* object,QEvent* event)override{auto* widget=qobject_cast<QWidget*>(object);if(window_&&widget&&widget->window()==window_&&(event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease||event->type()==QEvent::MouseButtonRelease||event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut))schedule();return false;}
public:
 FormulaBarControls(MainWindow& window,WorksheetView& view,QLineEdit& name,QLineEdit& formula):QObject(&window),window_(&window),view_(&view),bar_(&formula){
  setObjectName("formula.controlsController");formula.setPlaceholderText(QString());auto* parent=formula.parentWidget();auto* layout=qobject_cast<QHBoxLayout*>(parent->layout());auto* fx=window.findChild<QToolButton*>("formula.insertFunction");if(!layout||!fx)return;while(auto* item=layout->takeAt(0))delete item;layout->setContentsMargins(7,8,6,10);layout->setSpacing(0);name.setFixedWidth(102);name.setFixedHeight(26);layout->addWidget(&name);layout->addSpacing(31);
  installNameList(name);auto* actions=new QWidget(parent);actions->setObjectName("formula.actions");actions->setFixedSize(110,26);actions->setStyleSheet("QWidget#formula\\.actions { background:white; border:1px solid #d5d5d5; } QToolButton { border:0; padding:0; background:transparent; } QToolButton:hover:enabled { background:#eeeeee; }");auto create=[&](const char* id,const char* label,bool accept){auto* button=new QToolButton(actions);button->setObjectName(id);button->setAccessibleName(label);button->setToolTip(label);button->setIcon(actionIcon(accept));button->setIconSize(QSize(20,20));button->setAutoRaise(true);button->setFocusPolicy(Qt::NoFocus);return button;};cancel_=create("formula.cancel","Cancel",false);accept_=create("formula.accept","Enter",true);cancel_->setGeometry(1,1,36,24);accept_->setGeometry(37,1,36,24);fx->setParent(actions);fx->setFixedSize(36,24);fx->move(73,1);fx->setFocusPolicy(Qt::NoFocus);fx->show();layout->addWidget(actions);layout->addSpacing(3);layout->addWidget(&formula,1);
  connect(cancel_,&QToolButton::clicked,this,[this]{if(!window_||!view_)return;if(!finishExternalReferenceEntry(*window_,false))view_->cancelEntry();refresh();});connect(accept_,&QToolButton::clicked,this,[this]{if(!window_||!view_)return;if(!finishExternalReferenceEntry(*window_,true)&&view_->commitPendingEditor(true))view_->setFocus();refresh();});connect(&formula,&QLineEdit::textChanged,this,[this]{schedule();});if(view.model())connect(view.model(),&QAbstractItemModel::dataChanged,this,[this]{schedule();});qApp->installEventFilter(this);refresh();
 }
 ~FormulaBarControls()override{qApp->removeEventFilter(this);}
};
}
QObject* installFormulaBarControls(MainWindow& window,WorksheetView& view,QLineEdit& name,QLineEdit& formula){if(auto* existing=window.findChild<QObject*>("formula.controlsController"))return existing;return new FormulaBarControls(window,view,name,formula);}
}
