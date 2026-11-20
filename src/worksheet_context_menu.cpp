#include "tigersheets/worksheet_context_menu.hpp"
#include "tigersheets/grid.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QScrollBar>
#include <QTableView>
namespace tigersheets {
namespace {
enum class Surface {Cells,Rows,Columns};
void showMenu(QTableView& view,WorksheetModel& model,Surface surface,const QPoint& point,
 const std::function<bool(const QString&,const Json&)>& execute){
 if(surface==Surface::Cells){
  const auto clicked=view.indexAt(point);if(!clicked.isValid())return;
  if(!view.selectionModel()->isSelected(clicked))view.setCurrentIndex(clicked);
 }else{
  auto* header=surface==Surface::Rows?view.verticalHeader():view.horizontalHeader();
  const int section=header->logicalIndexAt(point);if(section<0)return;
  const bool selected=surface==Surface::Rows?view.selectionModel()->isRowSelected(section):view.selectionModel()->isColumnSelected(section);
  if(!selected){
   const auto first=model.index(surface==Surface::Rows?section:0,surface==Surface::Rows?0:section);
   const auto last=model.index(surface==Surface::Rows?section:model.rowCount()-1,surface==Surface::Rows?model.columnCount()-1:section);
   const auto current=view.currentIndex();const int horizontal=view.horizontalScrollBar()->value(),vertical=view.verticalScrollBar()->value();
   const auto focus=model.index(surface==Surface::Rows?section:(current.isValid()?current.row():0),surface==Surface::Rows?(current.isValid()?current.column():0):section);
   view.selectionModel()->setCurrentIndex(focus,QItemSelectionModel::NoUpdate);view.selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);
   view.horizontalScrollBar()->setValue(horizontal);view.verticalScrollBar()->setValue(vertical);
  }
 }
 auto* menu=new QMenu(&view);menu->setObjectName("worksheet.contextMenu");menu->setAttribute(Qt::WA_DeleteOnClose);menu->setToolTipsVisible(true);
 const auto revision=model.workbookView().at("revision");const auto sheet=model.sheetId();
 const auto selection=view.selectionModel()->selection();const auto current=view.currentIndex();
 const bool editable=!model.workbookView().is_null()&&(model.flags(view.currentIndex())&Qt::ItemIsEditable);
 const bool rectangle=view.selectionModel()->selection().size()==1;
 const auto selectedCells=rectangle?static_cast<qint64>(selection.front().width())*selection.front().height():0;
 const bool clipboardBounded=rectangle&&(selectedCells<=1000000||(selection.front().top()==0&&selection.front().height()==model.rowCount())||(selection.front().left()==0&&selection.front().width()==model.columnCount()));
 const bool formatBounded=rectangle&&selectedCells<=100000;
 qint64 borderCells=0;for(const auto& range:selection)borderCells+=static_cast<qint64>(range.width())*range.height();const bool formatDialogBounded=!selection.isEmpty()&&borderCells<=100000;
 const auto clipboard=QApplication::clipboard()->mimeData();const bool pasteable=clipboard&&(clipboard->hasText()||clipboard->hasFormat("application/x-tigersheets-cells+json"));
 auto add=[&](QMenu* parent,const QString& text,const QString& command,const Json& args=Json::object(),bool enabled=true,const QString& shortcut=QString()){
  auto* action=parent->addAction(text);action->setObjectName("context."+command);action->setEnabled(enabled);
  if(!shortcut.isEmpty())action->setShortcut(QKeySequence(shortcut));
  QObject::connect(action,&QAction::triggered,&view,[&view,&model,menu,sheet,revision,selection,current,execute,command,args]{
   if(menu->property("invalidated").toBool()||model.sheetId()!=sheet||model.workbookView().at("revision")!=revision||view.currentIndex()!=current||view.selectionModel()->selection()!=selection)return;
   execute(command,args);
  });return action;
 };
 QObject::connect(&model,&QAbstractItemModel::modelAboutToBeReset,menu,[menu]{menu->setProperty("invalidated",true);menu->close();});
 const auto invalidate=[menu]{menu->setProperty("invalidated",true);menu->close();};
 QObject::connect(view.selectionModel(),&QItemSelectionModel::selectionChanged,menu,invalidate);
 QObject::connect(view.selectionModel(),&QItemSelectionModel::currentChanged,menu,invalidate);
 QObject::connect(&model,&QAbstractItemModel::dataChanged,menu,[&model,menu,sheet,revision]{if(model.sheetId()!=sheet||model.workbookView().at("revision")!=revision){menu->setProperty("invalidated",true);menu->close();}});
 for(auto* action:{add(menu,"Cu&t","cut",Json::object(),editable&&clipboardBounded,"Ctrl+X"),add(menu,"&Copy","copy",Json::object(),clipboardBounded,"Ctrl+C")})if(rectangle&&!clipboardBounded)action->setToolTip("Select at most 1,000,000 cells for clipboard operations.");
 add(menu,"&Paste","paste",Json::object(),editable&&rectangle&&pasteable,"Ctrl+V");
 auto* paste=menu->addMenu("Paste &Special");paste->setEnabled(editable&&rectangle&&pasteable);
 add(paste,"Paste Special…","paste.special");add(paste,"Values","paste.values",{{"mode","values"}});add(paste,"Formulas","paste.formulas",{{"mode","formulas"}});add(paste,"Formats","paste.formats",{{"mode","formats"}});add(paste,"Transpose","paste.transpose",{{"mode","transpose"}});
 menu->addSeparator();auto* insert=menu->addMenu("&Insert");auto* remove=menu->addMenu("&Delete");
 if(surface!=Surface::Columns){add(insert,"Insert Rows Above","rows.insertAbove",Json::object(),editable&&rectangle);add(insert,"Insert Rows Below","rows.insertBelow",Json::object(),editable&&rectangle);add(remove,"Delete Rows","rows.delete",Json::object(),editable&&rectangle);}
 if(surface!=Surface::Rows){add(insert,"Insert Columns Left","columns.insertLeft",Json::object(),editable&&rectangle);add(insert,"Insert Columns Right","columns.insertRight",Json::object(),editable&&rectangle);add(remove,"Delete Columns","columns.delete",Json::object(),editable&&rectangle);}
 add(menu,"Clear Contents","clear",Json::object(),editable&&rectangle,"Del");add(menu,"Clear Formats","cells.clearFormats",Json::object(),editable&&formatBounded);
 if(surface==Surface::Cells){
  menu->addSeparator();auto* merge=menu->addMenu("&Merge Cells");
  const auto ranges=view.selectionModel()->selection();const bool multiple=rectangle&&(ranges.front().width()>1||ranges.front().height()>1);
  add(merge,"Merge && Center","cells.mergeCenter",Json::object(),editable&&multiple&&formatBounded);
  add(merge,"Merge Cells","cells.merge",Json::object(),editable&&multiple&&formatBounded);add(merge,"Unmerge Cells","cells.unmerge",Json::object(),editable&&rectangle);
  auto* sort=menu->addMenu("S&ort");add(sort,"Sort Smallest to Largest / A to Z","sort.ascending",Json::object(),editable&&rectangle);add(sort,"Sort Largest to Smallest / Z to A","sort.descending",Json::object(),editable&&rectangle);
  add(menu,"Edit Note…","note.edit",Json::object(),editable&&formatBounded);
 }else{
  menu->addSeparator();const bool rows=surface==Surface::Rows;
  add(menu,rows?"Row Height…":"Column Width…",rows?"rows.size":"columns.size",Json::object(),editable&&rectangle);
  add(menu,"Hide",rows?"rows.hide":"columns.hide",Json::object(),editable&&rectangle);
  add(menu,"Unhide",rows?"rows.unhide":"columns.unhide",Json::object(),editable&&rectangle);
 }
 menu->addSeparator();add(menu,"&Format Cells…","cells.format",Json::object(),editable&&formatDialogBounded,"Ctrl+1");
 if(rectangle&&!formatBounded)for(auto* action:menu->findChildren<QAction*>())if(action->objectName()=="context.cells.format"||action->objectName()=="context.cells.clearFormats"||action->objectName()=="context.cells.merge"||action->objectName()=="context.cells.mergeCenter"||action->objectName()=="context.note.edit")action->setToolTip("Select at most 100,000 cells for formatting, notes or merging.");
 auto* target=surface==Surface::Cells?view.viewport():(surface==Surface::Rows?view.verticalHeader()->viewport():view.horizontalHeader()->viewport());
 menu->popup(target->mapToGlobal(point));
}
class MenuKeys final:public QObject {
public:
 MenuKeys(QTableView& view,WorksheetModel& model,std::function<bool(const QString&,const Json&)> execute):QObject(&view),view_(view),model_(model),execute_(std::move(execute)){view.installEventFilter(this);}
 bool eventFilter(QObject* object,QEvent* event)override{
  if(object==&view_&&event->type()==QEvent::KeyPress){const auto* key=static_cast<QKeyEvent*>(event);if((key->key()==Qt::Key_F10&&key->modifiers()==Qt::ShiftModifier)||(key->key()==Qt::Key_Menu&&key->modifiers()==Qt::NoModifier)){
   if(view_.currentIndex().isValid()){view_.scrollTo(view_.currentIndex());showMenu(view_,model_,Surface::Cells,view_.visualRect(view_.currentIndex()).center(),execute_);}return true;
  }}return QObject::eventFilter(object,event);
 }
private:QTableView& view_;WorksheetModel& model_;std::function<bool(const QString&,const Json&)> execute_;
};
}
void installWorksheetContextMenus(QTableView& view,WorksheetModel& model,std::function<bool(const QString&,const Json&)> execute){
 new MenuKeys(view,model,execute);
 view.setContextMenuPolicy(Qt::CustomContextMenu);
 QObject::connect(&view,&QWidget::customContextMenuRequested,&view,[&view,&model,execute](const QPoint& point){showMenu(view,model,Surface::Cells,point,execute);});
 for(auto surface:{Surface::Rows,Surface::Columns}){
  auto* header=surface==Surface::Rows?view.verticalHeader():view.horizontalHeader();header->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(header,&QWidget::customContextMenuRequested,&view,[&view,&model,execute,surface](const QPoint& point){showMenu(view,model,surface,point,execute);});
 }
}
void invalidateWorksheetContextMenus(QTableView& view){
 for(auto* menu:view.findChildren<QMenu*>("worksheet.contextMenu")){menu->setProperty("invalidated",true);menu->close();}
}
}
