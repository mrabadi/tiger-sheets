#include "tigersheets/external_reference_entry.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/formula_editor.hpp"
#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QObject>
#include <QPointer>
#include <QStatusBar>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabBar>
#include <algorithm>
namespace tigersheets {
namespace {
class ExternalReferenceEntry;
struct Entry {
 QPointer<ExternalReferenceEntry> origin;
 QPointer<ExternalReferenceEntry> activeSurface;
 QPersistentModelIndex originIndex;
 QString originSheet;
 Json sources=Json::array();
 std::string pointedNumberFormat,pointedReference;
 int start=-1,length=0;
 bool inserting=false,forwarding=false,originWasInline=false;
};
Entry& entry(){static Entry value;return value;}
QList<QPointer<ExternalReferenceEntry>>& windows(){static QList<QPointer<ExternalReferenceEntry>> value;return value;}
class ExternalReferenceEntry final:public QObject {
 MainWindow& window_;WorksheetView& view_;QLineEdit& bar_;
 QPersistentModelIndex dragAnchor_,pointAnchor_,pointExtent_;QPointer<QLabel> rangeHint_;bool keepAnchor_=false;
 void context(const QString& mode){view_.setProperty("worksheet.entryMode",mode);if(auto* status=window_.findChild<QLabel*>("status.saveState"))status->setText(mode);if(auto* name=window_.findChild<QLineEdit*>("formula.name")){const auto index=view_.currentIndex();name->setText(entry().origin==this?QString("SUM"):QString::fromStdString(columnName(index.column()+1))+QString::number(index.row()+1));}}
 void hideRangeHint(){if(rangeHint_)rangeHint_->hide();}
 void showRangeHint(const QModelIndex& first,const QModelIndex& last){
  if(first==last){hideRangeHint();return;}
  if(!rangeHint_){rangeHint_=new QLabel(view_.viewport());rangeHint_->setObjectName("worksheet.pointRangeHint");rangeHint_->setFocusPolicy(Qt::NoFocus);rangeHint_->setAttribute(Qt::WA_TransparentForMouseEvents);rangeHint_->setStyleSheet("color:#000000;background:#ffffff;border:1px solid #767676;padding:0px 3px;");}
  rangeHint_->setText(QString::number(std::abs(first.row()-last.row())+1)+"R x "+QString::number(std::abs(first.column()-last.column())+1)+"C");rangeHint_->setFixedSize(std::max(46,rangeHint_->fontMetrics().horizontalAdvance(rangeHint_->text())+8),20);
  const auto bottomRight=view_.model()->index(std::max(first.row(),last.row()),std::max(first.column(),last.column()));rangeHint_->move(view_.visualRect(bottomRight).bottomRight()+QPoint(4,4));rangeHint_->show();rangeHint_->raise();
 }
 void mirrorDraft(){
  auto* origin=entry().origin.data();if(!origin||origin==this)return;
  view_.setProperty("worksheet.externalPointing",true);bar_.setText(origin->bar_.text());bar_.setCursorPosition(origin->bar_.cursorPosition());bar_.setModified(false);
  if(const auto* model=dynamic_cast<const WorksheetModel*>(origin->view_.model());model&&entry().originIndex.isValid())view_.setProperty("worksheet.entryFontSize",model->cellStyle(entry().originIndex.row(),entry().originIndex.column()).value("fontSize",11.0));
  if(auto* state=origin->window_.findChild<QLabel*>("status.saveState"))context(state->text());
  view_.viewport()->update();
 }
 void restoreSourceDrafts(){for(const auto& surface:windows())if(surface&&surface!=entry().origin){surface->hideRangeHint();surface->dragAnchor_=QPersistentModelIndex();surface->pointAnchor_=QPersistentModelIndex();surface->pointExtent_=QPersistentModelIndex();surface->view_.setProperty("worksheet.externalPointing",false);surface->view_.setProperty("worksheet.externalPointRange",QVariant());surface->view_.setProperty("worksheet.entryFontSize",QVariant());surface->view_.setProperty("worksheet.entryMode","Ready");surface->view_.selectionModel()->select(surface->view_.currentIndex(),QItemSelectionModel::ClearAndSelect);surface->bar_.setModified(false);surface->bar_.setText(surface->view_.model()->data(surface->view_.currentIndex(),Qt::EditRole).toString());if(auto* status=surface->window_.findChild<QLabel*>("status.saveState"))status->setText("Ready");if(auto* name=surface->window_.findChild<QLineEdit*>("formula.name")){const auto index=surface->view_.currentIndex();name->setText(QString::fromStdString(columnName(index.column()+1))+QString::number(index.row()+1));}}}
 void releaseDraftFocus(){const bool modified=bar_.isModified();bar_.setModified(false);bar_.clearFocus();bar_.setModified(modified);}
 void activate(bool editing=false){window_.raise();window_.activateWindow();if(editing)bar_.setFocus();else view_.setFocus();}
 bool prepareWindowChange(){
  const bool wasInline=view_.activeCellEditor();
  const auto originIndex=view_.currentIndex();const auto* model=dynamic_cast<WorksheetModel*>(view_.model());const auto originSheet=model?model->sheetId():QString();
  if(!entry().origin&&view_.prepareReferenceWorkbookChange()){
   entry().origin=this;entry().originIndex=originIndex;entry().originSheet=originSheet;
   entry().originWasInline=wasInline;
   view_.selectionModel()->setCurrentIndex(originIndex,QItemSelectionModel::NoUpdate);
  }else if(entry().origin==this&&wasInline)view_.prepareReferenceWorkbookChange();
  return entry().origin==this;
 }
 void switchWindow(bool reverse){
  hideRangeHint();dragAnchor_=QPersistentModelIndex();
  auto& registry=windows();registry.removeIf([](const auto& item){return !item;});
  QList<ExternalReferenceEntry*> visible;for(const auto& item:registry)if(item->window_.isVisible())visible.push_back(item);
  if(visible.size()<2)return;const auto index=visible.indexOf(this);if(index<0)return;
  prepareWindowChange();
  if(auto* origin=entry().origin.data())origin->releaseDraftFocus();
  const auto next=(index+(reverse?-1:1)+visible.size())%visible.size();auto* destination=visible[next];entry().activeSurface=destination;if(entry().origin&&destination!=entry().origin)destination->mirrorDraft();destination->activate(bool(entry().origin));
 }
 void point(const QModelIndex& index,const QModelIndex& end=QModelIndex()){
  auto* origin=entry().origin.data();if(!origin)return;auto* model=dynamic_cast<WorksheetModel*>(view_.model());if(!model)return;
  const auto path=window_.property("workbook.path").toString().toStdString();
  if(path.empty()){mirrorDraft();window_.statusBar()->showMessage("Save this source workbook before linking to it.");return;}
  QString sheet;for(const auto& item:model->workbookView().at("sheets"))if(item.at("id")==model->sheetId().toStdString()){sheet=QString::fromStdString(item.at("name").get<std::string>());break;}
  auto name=window_.property("workbook.name").toString();if(name.isEmpty())name=QFileInfo(window_.property("workbook.path").toString()).fileName();if(name.isEmpty())if(auto* label=window_.findChild<QLabel*>("workspace.documentName"))name=label->text();
  if(!path.empty()&&std::none_of(entry().sources.begin(),entry().sources.end(),[&](const Json& source){return source.at("path")==path;}))entry().sources.push_back({{"name",name.toStdString()},{"path",path},{"enabled",true}});
  const auto other=end.isValid()?end:index;const auto first=model->index(std::min(index.row(),other.row()),std::min(index.column(),other.column())),last=model->index(std::max(index.row(),other.row()),std::max(index.column(),other.column()));
  entry().pointedNumberFormat=model->cellStyle(first.row(),first.column()).value("numberFormat",std::string("General"));
  if(first!=last){
   // Fail closed for large or nonnumeric selections instead of scanning an
   // entire worksheet during every pointer move.
   const auto count=static_cast<qint64>(last.row()-first.row()+1)*(last.column()-first.column()+1);
   if(count>4096)entry().pointedNumberFormat.clear();
   else for(int row=first.row();row<=last.row()&&!entry().pointedNumberFormat.empty();++row)for(int col=first.column();col<=last.column();++col)if(!model->numericValue(row,col)||model->cellStyle(row,col).value("numberFormat",std::string("General"))!=entry().pointedNumberFormat){entry().pointedNumberFormat.clear();break;}
  }
  view_.setProperty("worksheet.externalPointRange",QRect(first.column(),first.row(),last.column()-first.column()+1,last.row()-first.row()+1));
  pointAnchor_=index;pointExtent_=other;
  auto address=[](const QModelIndex& cell){return "$"+QString::fromStdString(columnName(cell.column()+1))+"$"+QString::number(cell.row()+1);};
  auto qualified="["+name+"]"+sheet;qualified.replace("'","''");auto reference="'"+qualified+"'!"+address(first);if(first!=last)reference+=":"+address(last);
  entry().pointedReference=reference.toStdString();
  view_.selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);
  auto& editor=origin->bar_;if(entry().start<0||editor.cursorPosition()!=entry().start+entry().length){entry().start=editor.hasSelectedText()?editor.selectionStart():editor.cursorPosition();entry().length=editor.selectedText().size();}
  entry().inserting=true;editor.setSelection(entry().start,entry().length);editor.insert(reference);editor.setModified(true);entry().length=reference.size();entry().inserting=false;origin->context("Point");mirrorDraft();origin->view_.viewport()->update();
 }
 void finish(QKeyEvent* key){
  auto* origin=entry().origin.data();if(!origin)return;if(origin->view_.activeCellEditor())origin->view_.prepareReferenceWorkbookChange();origin->releaseDraftFocus();origin->activate();
  if(key->key()==Qt::Key_Escape)origin->view_.cancelEntry();
  else{
   auto* model=dynamic_cast<WorksheetModel*>(origin->view_.model());if(!model||!entry().originIndex.isValid())return;
   const auto index=entry().originIndex;const auto& book=origin->window_.currentSession().snapshotView();const auto priorLinks=book.value("links",Json::array());auto links=priorLinks;
   for(const auto& source:entry().sources)if(std::none_of(links.begin(),links.end(),[&](const Json& linked){return linked.value("path",std::string())==source.at("path");}))links.push_back(source);
   Json operations=Json::array();if(links!=priorLinks)operations.push_back({{"type","set_metadata"},{"key","links"},{"value",links}});
   const auto formula=origin->bar_.text().toStdString();Json cell={{"type","set_cell"},{"sheet",entry().originSheet.toStdString()},{"row",index.row()+1},{"col",index.column()+1},{"input",formula}};
   // The visible model may have changed sheets during Point mode. Never infer
   // an origin format from another sheet at the same coordinate.
   if(model->sheetId()==entry().originSheet){
   const auto references=formulaReferences(formula);
   if(!references.empty()&&!references.front().external)if(const auto format=model->inferredFormulaNumberFormat(index,origin->bar_.text()))cell["style"]={{"numberFormat",*format}};
   const auto targetFormat=model->cellStyle(index.row(),index.column()).value("numberFormat",std::string("General"));
   if((targetFormat.empty()||targetFormat=="General")&&!entry().pointedNumberFormat.empty()&&entry().pointedNumberFormat!="General"&&entry().pointedNumberFormat!="@"){
    if(references.size()==1&&references.front().external){const auto& reference=references.front();
     const bool direct=entry().start==1&&entry().length==origin->bar_.text().size()-1&&reference.start==1&&reference.length==formula.size()-1&&reference.row1==reference.row2&&reference.col1==reference.col2;
     auto unlocked=[](std::string text){const auto separator=text.rfind('!');if(separator!=std::string::npos)text.erase(std::remove(text.begin()+separator+1,text.end(),'$'),text.end());return text;};
     const auto prefix=QString::fromStdString(formula.substr(0,reference.start)).trimmed(),suffix=QString::fromStdString(formula.substr(reference.start+reference.length)).trimmed();
     const bool sum=(reference.row1!=reference.row2||reference.col1!=reference.col2)&&prefix.compare("=SUM(",Qt::CaseInsensitive)==0&&suffix==")"&&unlocked(formula.substr(reference.start,reference.length))==unlocked(entry().pointedReference);
     if(direct||sum)cell["style"]={{"numberFormat",entry().pointedNumberFormat}};
    }
   }
   }
   operations.push_back(std::move(cell));
   if(!model->apply(operations)){origin->window_.statusBar()->showMessage(model->lastError());return;}
   origin->bar_.setModified(false);origin->view_.cancelEntry();
   const bool retain=keepAnchor_||((key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter)&&(key->modifiers()&Qt::ControlModifier));const int direction=retain?0:(key->modifiers()&Qt::ShiftModifier)?-1:1;const auto next=model->index(std::clamp(index.row()+direction,0,model->rowCount()-1),index.column());origin->view_.setCurrentIndex(next);origin->view_.scrollTo(next);origin->view_.setFocus();
  }
  restoreSourceDrafts();entry()=Entry{};
 }
 bool eventFilter(QObject* object,QEvent* event)override {
  auto* widget=qobject_cast<QWidget*>(object);if(!widget||widget->window()!=&window_||entry().forwarding)return false;
  if(object==&window_&&event->type()==QEvent::WindowDeactivate&&!view_.property("worksheet.draftWarning").toBool()&&!QApplication::activeModalWidget()&&!QApplication::activePopupWidget()){
   if(prepareWindowChange())releaseDraftFocus();
  }
  if(object==&window_&&event->type()==QEvent::WindowActivate&&entry().origin&&entry().origin!=this){
   entry().activeSurface=this;mirrorDraft();
  }
  if(event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);
   if((key->modifiers()&Qt::ControlModifier)&&!(key->modifiers()&(Qt::AltModifier|Qt::MetaModifier))&&(key->key()==Qt::Key_Tab||key->key()==Qt::Key_F6)){switchWindow(key->modifiers()&Qt::ShiftModifier);return true;}
   if(qobject_cast<QScrollBar*>(widget))return false;
   if(entry().origin==this&&entry().originWasInline&&key->key()==Qt::Key_F2&&!view_.activeCellEditor()){
    if(auto* editor=view_.resumeReferenceCellEditor()){
     entry().forwarding=true;view_.handleEditorKey(editor,key);entry().forwarding=false;return true;
    }
   }
   if(entry().origin&&entry().origin!=this){
    if((key->modifiers()&Qt::ControlModifier)&&!(key->modifiers()&(Qt::AltModifier|Qt::MetaModifier))&&(key->key()==Qt::Key_PageUp||key->key()==Qt::Key_PageDown)){
     if(auto* tabs=window_.findChild<QTabBar*>("workspace.sheets"))tabs->setCurrentIndex(std::clamp(tabs->currentIndex()+(key->key()==Qt::Key_PageUp?-1:1),0,tabs->count()-1));mirrorDraft();bar_.setFocus();return true;
    }
    const bool arrow=key->key()==Qt::Key_Left||key->key()==Qt::Key_Right||key->key()==Qt::Key_Up||key->key()==Qt::Key_Down;
    if(arrow&&!(key->modifiers()&(Qt::AltModifier|Qt::MetaModifier))){
     if(auto* editor=dynamic_cast<FormulaLineEdit*>(&bar_);editor&&editor->completionPopup()->isVisible()&&editor->handleFormulaKey(key))return true;
     const auto cursor=pointExtent_.isValid()?QModelIndex(pointExtent_):view_.currentIndex();QModelIndex next;
     if(key->modifiers()&Qt::ControlModifier){const auto prior=view_.currentIndex();const QSignalBlocker blocked(view_.selectionModel());view_.selectionModel()->setCurrentIndex(cursor,QItemSelectionModel::NoUpdate);entry().forwarding=true;QKeyEvent navigation(key->type(),key->key(),key->modifiers(),key->text(),key->isAutoRepeat(),key->count());QApplication::sendEvent(&view_,&navigation);entry().forwarding=false;next=view_.currentIndex();view_.selectionModel()->setCurrentIndex(prior,QItemSelectionModel::NoUpdate);}
     else next=view_.model()->index(std::clamp(cursor.row()+(key->key()==Qt::Key_Down?1:key->key()==Qt::Key_Up?-1:0),0,view_.model()->rowCount()-1),std::clamp(cursor.column()+(key->key()==Qt::Key_Right?1:key->key()==Qt::Key_Left?-1:0),0,view_.model()->columnCount()-1));
     point((key->modifiers()&Qt::ShiftModifier)&&pointAnchor_.isValid()?QModelIndex(pointAnchor_):next,next);view_.scrollTo(next);mirrorDraft();return true;
    }
   }
   if(entry().origin&&(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter||key->key()==Qt::Key_Escape)){
    finish(key);return true;
   }
   if(entry().origin&&entry().origin!=this&&!(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&
      (key->key()==Qt::Key_F4||key->key()==Qt::Key_Backspace||key->key()==Qt::Key_Delete||key->key()==Qt::Key_Home||key->key()==Qt::Key_End||(!key->text().isEmpty()&&key->text().front().isPrint()))){
    auto* origin=entry().origin.data();const auto before=origin->bar_.text().size();const bool cycle=key->key()==Qt::Key_F4;
    entry().forwarding=true;entry().inserting=cycle;origin->bar_.setProperty("formula.cyclePointedRange",cycle);QKeyEvent forwarded(key->type(),key->key(),key->modifiers(),key->text(),key->isAutoRepeat(),key->count());QApplication::sendEvent(&origin->bar_,&forwarded);origin->bar_.setProperty("formula.cyclePointedRange",false);entry().forwarding=false;entry().inserting=false;
    if(cycle&&entry().start>=0){entry().length+=origin->bar_.text().size()-before;origin->context("Point");}
    mirrorDraft();
    return true;
   }
  }
  if(event->type()==QEvent::MouseButtonPress&&object==view_.viewport()&&entry().origin==this&&entry().originWasInline&&!view_.activeCellEditor()){
   auto* mouse=static_cast<QMouseEvent*>(event);if(mouse->button()==Qt::LeftButton&&mouse->modifiers()==Qt::NoModifier&&view_.indexAt(mouse->position().toPoint())==entry().originIndex){
    const auto* status=window_.findChild<QLabel*>("status.saveState");const auto mode=status?status->text():QString("Point");if(view_.resumeReferenceCellEditor()){context(mode);return true;}
   }
  }
  if(event->type()==QEvent::MouseButtonPress&&object==view_.viewport()&&entry().origin&&entry().origin!=this){
   auto* mouse=static_cast<QMouseEvent*>(event);const auto index=view_.indexAt(mouse->position().toPoint());if(mouse->button()==Qt::LeftButton&&index.isValid()){dragAnchor_=index;hideRangeHint();point(index);return true;}
  }
  if((event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonRelease)&&object==view_.viewport()&&dragAnchor_.isValid()&&entry().origin&&entry().origin!=this){
   auto* mouse=static_cast<QMouseEvent*>(event);const auto index=view_.indexAt(mouse->position().toPoint());if(index.isValid()){point(dragAnchor_,index);if(event->type()==QEvent::MouseMove)showRangeHint(dragAnchor_,index);}
   if(event->type()==QEvent::MouseButtonRelease){hideRangeHint();dragAnchor_=QPersistentModelIndex();}return true;
  }
  return false;
 }
public:
 bool finishFromControl(bool accept){if(!entry().origin)return false;keepAnchor_=true;QKeyEvent key(QEvent::KeyPress,accept?Qt::Key_Return:Qt::Key_Escape,Qt::NoModifier);finish(&key);keepAnchor_=false;return true;}
 ExternalReferenceEntry(MainWindow& window,WorksheetView& view,QLineEdit& bar):QObject(&window),window_(window),view_(view),bar_(bar){
  setObjectName("worksheet.externalReferenceEntry");windows().push_back(this);qApp->installEventFilter(this);
  auto resetPointSurface=[this]{if(view_.property("worksheet.externalPointing").toBool()){hideRangeHint();dragAnchor_=QPersistentModelIndex();pointAnchor_=QPersistentModelIndex();pointExtent_=QPersistentModelIndex();view_.setProperty("worksheet.externalPointRange",QVariant());view_.viewport()->update();}};
  connect(view_.model(),&QAbstractItemModel::modelReset,this,resetPointSurface);
  if(auto* tabs=window_.findChild<QTabBar*>("workspace.sheets"))connect(tabs,&QTabBar::currentChanged,this,resetPointSurface);
  connect(&bar_,&QLineEdit::textChanged,this,[this]{if(entry().origin==this){if(!entry().inserting){entry().start=-1;entry().length=0;}if(entry().activeSurface&&entry().activeSurface!=this)entry().activeSurface->mirrorDraft();}});
 }
 ~ExternalReferenceEntry()override{qApp->removeEventFilter(this);if(entry().origin==this){restoreSourceDrafts();entry()=Entry{};}}
};
}
bool finishExternalReferenceEntry(MainWindow& window,bool accept){auto* controller=dynamic_cast<ExternalReferenceEntry*>(window.findChild<QObject*>("worksheet.externalReferenceEntry"));return controller&&controller->finishFromControl(accept);}
QObject* installExternalReferenceEntry(MainWindow& window,WorksheetView& view,QLineEdit& bar){
 if(auto* existing=window.findChild<QObject*>("worksheet.externalReferenceEntry"))return existing;
 return new ExternalReferenceEntry(window,view,bar);
}
}
