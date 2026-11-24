#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QTableView>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QToolButton>
#include <QTabWidget>
#include <QTabBar>
#include <QtTest/QTest>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
namespace {
Json importedMergeFixture(){return {{"revision",0},{"sheets",Json::array({{{"id","cash"},{"name","Consolidated Cash"},{"mergedRanges",Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",1}},{{"row1",1},{"col1",2},{"row2",1},{"col2",4}}})},{"cells",Json::array({{{"row",1},{"col",1},{"input","Cash flow"}},{{"row",1},{"col",2},{"input","Periods"},{"style",{{"alignment","center"},{"wrap",true}}}},{{"row",2},{"col",2},{"input","2025"}},{{"row",3},{"col",2},{"input","=1+2"}}})}}})}};}
struct ImportedMergeFixture {
 QTemporaryDir directory;MainWindow window;QTableView* grid;WorksheetModel* model;Json before;
 ImportedMergeFixture(const Json& book=importedMergeFixture()){const auto path=directory.filePath("imported.xlsx");writeWorkbook(path.toStdString(),book);window.resize(1200,760);window.show();require(window.openWorkbook(path),"open an actual XLSX with adjacent vertical and horizontal imported merges");QApplication::processEvents();grid=window.findChild<QTableView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(grid->model());require(grid->columnSpan(0,1)==3,"imported B1:D1 is displayed as one merged cell");grid->setCurrentIndex(model->index(1,1));before=window.currentSession().snapshot();}
 void clickHeader(int column){const QPoint point(grid->columnViewportPosition(column)+grid->columnWidth(column)/2,grid->rowViewportPosition(0)+grid->rowHeight(0)/2);QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,point);QApplication::processEvents();}
 void verify(const Json& remaining=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",1}}})){const auto sheet=window.currentSession().snapshotView().at("sheets")[0];require(sheet.at("mergedRanges")==remaining&&grid->columnSpan(0,1)==1,"unmerge removes exactly the selected imported merges");require(sheet.at("cells")==before.at("sheets")[0].at("cells"),"unmerge preserves every imported value, formula, and style");require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"one Undo restores exact imported merges and content");require(window.executeCommand("redo")&&grid->columnSpan(0,1)==1,"Redo unmerges the imported header again");const auto output=directory.filePath("unmerged.xlsx");require(window.saveWorkbook(output),"save unmerged imported workbook to another local file");const auto saved=readWorkbook(output.toStdString());require(saved.workbook.at("sheets")[0].value("mergedRanges",Json::array())==sheet.at("mergedRanges"),"saved XLSX retains unmerge after reopening");}
};
QAction* findUnmerge(QMenu* menu){for(auto* item:menu->actions()){if(item->objectName()=="context.cells.unmerge")return item;if(item->menu())if(auto* nested=findUnmerge(item->menu()))return nested;}return nullptr;}
void importedFormatUnmerge(int selectionKind=0){
 for(int route=0;route<3;++route){auto book=importedMergeFixture();const Json gap={{"row1",4},{"col1",5},{"row2",5},{"col2",5}};if(selectionKind){book["sheets"][0]["mergedRanges"].push_back(gap);book["sheets"][0]["mergedRanges"].push_back({{"row1",7},{"col1",6},{"row2",7},{"col2",8}});}ImportedMergeFixture f(book);f.clickHeader(2);if(selectionKind==1)QTest::mouseClick(f.grid->viewport(),Qt::LeftButton,Qt::ControlModifier,f.grid->visualRect(f.model->index(6,5)).center());else if(selectionKind==2){auto* header=f.grid->horizontalHeader();QTest::mouseClick(header->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(header->sectionViewportPosition(2)+5,5));QTest::mouseClick(header->viewport(),Qt::LeftButton,Qt::ControlModifier,QPoint(header->sectionViewportPosition(5)+5,5));}QApplication::processEvents();bool opened=false;std::exception_ptr error;QTimer dialogAction;dialogAction.setInterval(0);QObject::connect(&dialogAction,&QTimer::timeout,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;dialogAction.stop();try{require(dialog,"native Format Cells opens for an imported merged header");opened=true;dialog->findChild<QTabWidget*>("formatCells.tabs")->setCurrentIndex(1);auto* merge=dialog->findChild<QCheckBox*>("formatCells.merge");require(merge&&merge->isEnabled()&&merge->checkState()==(selectionKind?Qt::PartiallyChecked:Qt::Checked),"Format Cells enables removing selected imported merges, including separate areas");QTest::mouseClick(merge,Qt::LeftButton,Qt::NoModifier,QPoint(8,merge->height()/2));require(merge->checkState()==Qt::Unchecked,"clicking the Merge cells checkbox selects unmerge");auto* buttons=dialog->findChild<QDialogButtonBox*>("formatCells.buttons");QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);}catch(...){error=std::current_exception();if(dialog)dialog->reject();}});dialogAction.start();
  if(route==0){f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_1,Qt::ControlModifier);QTest::keyRelease(f.grid,Qt::Key_Control);}else if(route==1){const auto point=f.grid->visualRect(f.model->index(0,1)).center();QContextMenuEvent event(QContextMenuEvent::Mouse,point,f.grid->viewport()->mapToGlobal(point));QApplication::sendEvent(f.grid->viewport(),&event);QApplication::processEvents();auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());QAction* format=nullptr;if(menu)for(auto* action:menu->actions())if(action->objectName()=="context.cells.format")format=action;require(format&&format->isEnabled(),"context Format Cells is available for the imported header");QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(format).center());}else{auto* launcher=f.window.findChild<QToolButton*>("home.alignmentDialog");require(launcher&&launcher->isEnabled(),"Home Alignment dialog launcher is available");QTest::mouseClick(launcher,Qt::LeftButton);}
  QApplication::processEvents();if(error)std::rethrow_exception(error);require(opened,"the chosen Format Cells workflow opened the native dialog");Json remaining=Json::array({{{"row1",1},{"col1",1},{"row2",2},{"col2",1}}});if(selectionKind)remaining.push_back(gap);f.verify(remaining);
 }
}
void importedAreaUnmerge(){
 for(int shape=0;shape<3;++shape)for(int route=0;route<2;++route){auto book=importedMergeFixture();const Json gap={{"row1",4},{"col1",2},{"row2",4},{"col2",4}};if(shape==2){book["sheets"][0]["mergedRanges"].push_back(gap);}if(shape>0)book["sheets"][0]["mergedRanges"].push_back({{"row1",7},{"col1",2},{"row2",7},{"col2",4}});ImportedMergeFixture f(book);
  if(shape==0){auto* header=f.grid->verticalHeader();QTest::mouseClick(header->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(5,header->sectionViewportPosition(0)+header->sectionSize(0)/2));}else if(shape==1){auto* header=f.grid->horizontalHeader();QTest::mouseClick(header->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(header->sectionViewportPosition(2)+header->sectionSize(2)/2,5));}else{f.clickHeader(1);QTest::mouseClick(f.grid->viewport(),Qt::LeftButton,Qt::ControlModifier,f.grid->visualRect(f.model->index(6,1)).center());}QApplication::processEvents();
  if(route==0)require(f.window.executeCommand("cells.unmerge"),"Unmerge accepts native whole-row/column expansion and Ctrl-selected merged areas");else{const auto selected=f.grid->selectionModel()->selection();const auto current=f.grid->currentIndex();f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_Menu);QApplication::processEvents();require(f.grid->selectionModel()->selection()==selected&&f.grid->currentIndex()==current,"opening the context menu on a merged span preserves the complete selected columns and current cell");auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());auto* unmerge=menu?findUnmerge(menu):nullptr;require(unmerge&&unmerge->isEnabled(),"context Unmerge is available for whole rows/columns and separate selected merged areas");unmerge->trigger();menu->close();QApplication::processEvents();}
  Json remaining=Json::array();if(shape!=0)remaining.push_back({{"row1",1},{"col1",1},{"row2",2},{"col2",1}});if(shape==2)remaining.push_back(gap);f.verify(remaining);
 }
}
void importedContextUnmerge(){
 for(int column:{1,2,3}){ImportedMergeFixture f;f.clickHeader(column);const auto point=f.grid->visualRect(f.model->index(0,1)).center();QContextMenuEvent event(QContextMenuEvent::Mouse,point,f.grid->viewport()->mapToGlobal(point));QApplication::sendEvent(f.grid->viewport(),&event);QApplication::processEvents();auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());require(menu,"context menu opens for an imported merged header");auto* unmerge=findUnmerge(menu);require(unmerge&&unmerge->isEnabled(),"Unmerge Cells is available after clicking any part of imported B1:D1");auto* submenu=qobject_cast<QMenu*>(unmerge->parent());require(submenu,"unmerge is in the native Merge Cells submenu");menu->setActiveAction(submenu->menuAction());QTest::keyClick(menu,Qt::Key_Right);QApplication::processEvents();require(submenu->isVisible(),"the native Merge Cells submenu opens");QTest::mouseClick(submenu,Qt::LeftButton,Qt::NoModifier,submenu->actionGeometry(unmerge).center());QApplication::processEvents();f.verify();}
}
}

int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 if(argc>1&&std::string(argv[1])=="imported_areas"){importedAreaUnmerge();std::cout<<"imported area unmerge passed\n";return 0;}
 if(argc>1&&std::string(argv[1])=="imported_format_areas"){importedFormatUnmerge(1);std::cout<<"imported disjoint Format Cells unmerge passed\n";return 0;}
 if(argc>1&&std::string(argv[1])=="imported_format_columns"){importedFormatUnmerge(2);std::cout<<"imported columns Format Cells unmerge passed\n";return 0;}
 if(argc>1&&std::string(argv[1])=="imported_format"){importedFormatUnmerge();std::cout<<"imported Format Cells unmerge passed\n";return 0;}
 if(argc>1&&std::string(argv[1])=="imported_context"){importedContextUnmerge();std::cout<<"imported context unmerge passed\n";return 0;}
 importedContextUnmerge();importedFormatUnmerge();importedFormatUnmerge(1);importedFormatUnmerge(2);importedAreaUnmerge();
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 model->setData(model->index(0,0),"Revenue",Qt::EditRole);model->setData(model->index(0,1),"Other value",Qt::EditRole);model->setData(model->index(1,1),"99",Qt::EditRole);grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(1,1)),QItemSelectionModel::ClearAndSelect);const auto before=window.currentSession().snapshot();
 require(window.executeCommand("cells.mergeCenter",{{"discardOtherValues",true}}),"Merge and Center executes on the selected rectangle");
 require(model->raw(0,0)=="Revenue"&&model->raw(0,1).isEmpty()&&model->raw(1,1).isEmpty()&&model->cellStyle(0,0).at("alignment")=="center","merge preserves only the upper-left value after explicit confirmation");
 require(grid->rowSpan(0,0)==2&&grid->columnSpan(0,0)==2,"merge is visible immediately");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"one Undo restores all values, formatting and merge state");
 require(window.executeCommand("redo"),"merge redo");grid->setCurrentIndex(model->index(0,0));require(window.executeCommand("cells.unmerge")&&grid->rowSpan(0,0)==1&&grid->columnSpan(0,0)==1&&model->raw(0,0)=="Revenue","Unmerge restores ordinary cells and retains the merged value");
 model->setData(model->index(0,1),"Retain me",Qt::EditRole);grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(0,1)),QItemSelectionModel::ClearAndSelect);const auto cancelled=window.currentSession().snapshot();bool warning=false;
 QTimer::singleShot(0,[&]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){warning=box->text().contains("upper-left");box->done(QMessageBox::No);}});
 require(!window.executeCommand("cells.merge")&&warning&&window.currentSession().snapshot()==cancelled,"declining the native merge warning preserves all values and undo state");
 require(window.executeCommand("cells.merge",{{"discardOtherValues",true}}),"merge cells without centering");grid->setCurrentIndex(model->index(0,1));grid->selectionModel()->select(QItemSelection(model->index(0,1),model->index(0,2)),QItemSelectionModel::ClearAndSelect);const auto partial=window.currentSession().snapshot();
 require(!window.executeCommand("cells.merge",{{"discardOtherValues",true}})&&window.currentSession().snapshot()==partial&&window.lastError().contains("whole existing merged cell"),"partially intersecting merged ranges are refused before losing values");
 std::cout<<"worksheet merge action tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
