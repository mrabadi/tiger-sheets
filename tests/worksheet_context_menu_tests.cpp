#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_context_menu.hpp"
#include <QApplication>
#include <QContextMenuEvent>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
QMenu* openMenu(QWidget* target,const QPoint& point){
 QContextMenuEvent event(QContextMenuEvent::Mouse,point,target->mapToGlobal(point));
 QApplication::sendEvent(target,&event);QApplication::processEvents();
 return qobject_cast<QMenu*>(QApplication::activePopupWidget());
}
QAction* action(QMenu* menu,const QString& name){
 if(!menu)return nullptr;for(auto* item:menu->actions()){if(item->objectName()=="context."+name)return item;if(item->menu())if(auto* nested=action(item->menu(),name))return nested;}return nullptr;
}
void cellMenu(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");
 auto* menu=openMenu(grid->viewport(),grid->visualRect(grid->model()->index(2,2)).center());
 require(menu,"right-clicking a worksheet cell opens its context menu");
 require(grid->currentIndex()==grid->model()->index(2,2),"right-click outside selection selects the clicked cell");
 for(const auto* command:{"cut","copy","paste","paste.special","rows.insertAbove","rows.insertBelow","columns.insertLeft","columns.insertRight","rows.delete","columns.delete","clear","cells.format","cells.merge","cells.unmerge"})require(action(menu,command),"cell context menu exposes familiar clipboard, structure, merge and format commands");
 menu->close();
}
void keyboardAndSelection(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=grid->model();
 model->setData(model->index(0,0),"Revenue",Qt::EditRole);model->setData(model->index(1,1),"120",Qt::EditRole);
 grid->setCurrentIndex(model->index(0,0));const QItemSelection selected(model->index(0,0),model->index(1,1));grid->selectionModel()->select(selected,QItemSelectionModel::ClearAndSelect);
 auto* menu=openMenu(grid->viewport(),grid->visualRect(model->index(1,1)).center());require(menu&&grid->selectionModel()->selection()==selected,"right-click inside a selection retains the complete selected rectangle");
 action(menu,"copy")->trigger();require(QApplication::clipboard()->text()=="Revenue\t\n\t120\n","context Copy operates on the complete retained selection");menu->close();QApplication::processEvents();
 grid->setFocus();QTest::keyClick(grid,Qt::Key_F10,Qt::ShiftModifier);QApplication::processEvents();menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
 require(menu&&action(menu,"copy"),"Shift+F10 opens the worksheet context menu from the keyboard");require(grid->selectionModel()->selection()==selected,"keyboard menu invocation preserves selection");menu->close();
 QApplication::processEvents();grid->setFocus();QTest::keyClick(grid,Qt::Key_Menu);QApplication::processEvents();menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());require(menu&&action(menu,"copy")&&grid->selectionModel()->selection()==selected,"Menu key opens the same worksheet menu and preserves its selection");menu->close();
}
void staleSelection(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=grid->model();model->setData(model->index(0,0),"Original",Qt::EditRole);model->setData(model->index(1,1),"Other",Qt::EditRole);grid->setCurrentIndex(model->index(0,0));
 auto* menu=openMenu(grid->viewport(),grid->visualRect(model->index(0,0)).center());require(menu,"selection safety menu opens");QPointer<QAction> copy=action(menu,"copy");
 QApplication::clipboard()->setText("retained clipboard");grid->setCurrentIndex(model->index(1,1));if(copy)copy->trigger();
 require(QApplication::clipboard()->text()=="retained clipboard","a retained menu action cannot target a later selection");if(menu->isVisible())menu->close();
}
void boundedClipboard(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.resize(800,600);view.setCurrentIndex(model.index(0,0));installWorksheetContextMenus(view,model,[](const QString&,const Json&){return true;});view.show();QApplication::processEvents();
 auto* header=view.horizontalHeader();auto* menu=openMenu(header->viewport(),QPoint(header->sectionViewportPosition(1)+10,5));require(menu,"column header menu opens");
 for(const auto* command:{"copy","cut"})require(action(menu,command)&&action(menu,command)->isEnabled(),"whole-column clipboard actions use the native sparse path");menu->close();QApplication::processEvents();view.selectionModel()->select(QItemSelection(model.index(0,0),model.index(1000,1000)),QItemSelectionModel::ClearAndSelect);menu=openMenu(view.viewport(),view.visualRect(model.index(0,0)).center());for(const auto* command:{"copy","cut"}){auto* item=action(menu,command);require(item&&!item->isEnabled(),"ordinary dense clipboard ranges retain their one-million-cell budget");require(item->toolTip().contains("1,000,000"),"disabled dense clipboard action explains the supported cell limit");}menu->close();
}
void frozenMenu(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=grid->model();model->setData(model->index(0,0),"Frozen revenue",Qt::EditRole);model->setData(model->index(4,0),"Unrelated",Qt::EditRole);grid->setCurrentIndex(model->index(4,0));require(window.executeCommand("view.freeze"),"freeze first row succeeds");QApplication::processEvents();
 auto* frozen=window.findChild<QTableView*>("worksheet.frozen");require(frozen,"frozen row has a named worksheet view");auto* menu=openMenu(frozen->viewport(),frozen->visualRect(model->index(0,0)).center());require(menu&&action(menu,"copy"),"right-click on frozen row opens worksheet clipboard actions");action(menu,"copy")->trigger();require(QApplication::clipboard()->text()=="Frozen revenue\n","frozen-row Copy targets the frozen cell rather than the main grid's prior selection");menu->close();QApplication::processEvents();
 menu=openMenu(frozen->viewport(),frozen->visualRect(model->index(0,0)).center());require(menu&&action(menu,"clear"),"frozen row menu reopens");action(menu,"clear")->trigger();require(model->data(model->index(0,0),Qt::EditRole).toString().isEmpty()&&model->data(model->index(4,0),Qt::EditRole).toString()=="Unrelated","frozen-row Clear affects the frozen selection only");menu->close();
}
void scrolledHeader(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.resize(800,600);installWorksheetContextMenus(view,model,[](const QString&,const Json&){return true;});view.show();QApplication::processEvents();
 for(const bool rows:{true,false}){view.setCurrentIndex(model.index(1000,50));view.scrollTo(view.currentIndex(),QAbstractItemView::PositionAtCenter);QApplication::processEvents();const int horizontal=view.horizontalScrollBar()->value(),vertical=view.verticalScrollBar()->value();const auto before=view.currentIndex();auto* header=rows?view.verticalHeader():view.horizontalHeader();const QPoint point=rows?QPoint(5,header->sectionViewportPosition(before.row())+5):QPoint(header->sectionViewportPosition(before.column())+5,5);auto* menu=openMenu(header->viewport(),point);require(menu,"scrolled header menu opens");require(view.horizontalScrollBar()->value()==horizontal&&view.verticalScrollBar()->value()==vertical,"right-clicking a scrolled header retains the visible scroll position");require(rows?view.currentIndex().column()==before.column():view.currentIndex().row()==before.row(),"header selection retains the current cross-axis coordinate");require(rows?view.selectionModel()->isRowSelected(before.row()):view.selectionModel()->isColumnSelected(before.column()),"header context selects the complete row or column");menu->close();QApplication::processEvents();}
}
void invalidatedWorkbook(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.resize(800,600);view.setCurrentIndex(model.index(0,0));int commands=0;installWorksheetContextMenus(view,model,[&](const QString&,const Json&){++commands;return true;});view.show();QApplication::processEvents();auto* menu=openMenu(view.viewport(),view.visualRect(model.index(0,0)).center());require(menu,"workbook invalidation menu opens");QPointer<QAction> copy=action(menu,"copy");const auto revision=model.workbookView().at("revision");invalidateWorksheetContextMenus(view);require(!menu->isVisible(),"workbook context replacement closes its menu");if(copy)copy->trigger();require(commands==0&&model.workbookView().at("revision")==revision,"explicit context invalidation refuses retained actions even when the replacement has the same sheet and revision");
}
void boundedFormatting(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.resize(800,600);view.setCurrentIndex(model.index(0,0));installWorksheetContextMenus(view,model,[](const QString&,const Json&){return true;});view.show();QApplication::processEvents();view.selectionModel()->select(QItemSelection(model.index(0,0),model.index(1000,99)),QItemSelectionModel::ClearAndSelect);auto* menu=openMenu(view.viewport(),view.visualRect(model.index(0,0)).center());require(menu,"large selected range menu opens");for(const auto* command:{"cells.format","cells.clearFormats","cells.merge","cells.mergeCenter","note.edit"}){auto* item=action(menu,command);require(item&&!item->isEnabled(),"dense formatting, notes and merge actions are disabled above their100,000-cell budget");require(item->toolTip().contains("100,000"),"disabled dense action explains the supported cell limit");}require(menu->toolTipsVisible(),"context menu displays explanations for disabled actions");require(action(menu,"clear")->isEnabled(),"sparse Clear Contents remains available for large selections");menu->close();QApplication::processEvents();view.selectionModel()->select(QItemSelection(model.index(0,0),model.index(0,1)),QItemSelectionModel::ClearAndSelect);menu=openMenu(view.viewport(),view.visualRect(model.index(0,0)).center());require(menu&&action(menu,"cells.format")->isEnabled()&&action(menu,"cells.merge")->isEnabled(),"ordinary formatting and merge remain available");menu->close();
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):std::string();if(selected.empty()||selected=="menu")cellMenu();if(selected.empty()||selected=="keyboard")keyboardAndSelection();if(selected.empty()||selected=="stale")staleSelection();if(selected.empty()||selected=="clipboard")boundedClipboard();if(selected.empty()||selected=="frozen")frozenMenu();if(selected.empty()||selected=="headers")scrolledHeader();if(selected.empty()||selected=="invalidation")invalidatedWorkbook();if(selected.empty()||selected=="bounds")boundedFormatting();std::cout<<"worksheet context menu tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
