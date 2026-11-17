#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <QFontMetrics>
#include <QLineEdit>
#include <QToolButton>
#include <QHeaderView>
#include <QMouseEvent>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void wholeColumnsShowHeldDimensions(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");
 require(model->setData(model->index(8,0),"Keep",Qt::EditRole),"prepare unrelated input for whole-column selection");view->setCurrentIndex(model->index(8,0));view->setFocus();const auto before=window.currentSession().snapshot();
 QTest::keyClick(view,Qt::Key_Space,Qt::ControlModifier);require(view->selectionModel()->selection().size()==1&&view->selectionModel()->selection().front().height()==model->rowCount(),"CtrlSpace selects the actual complete column");
 for(int n=0;n<5;++n){QTest::keyPress(view,Qt::Key_Right,Qt::ShiftModifier);QTest::keyRelease(view,Qt::Key_Right);}
 const auto selected=view->selectionModel()->selection();require(selected.size()==1&&selected.front().left()==0&&selected.front().width()==6&&selected.front().height()==model->rowCount(),"held ShiftRight extends actual whole-column selection to A:F");require(view->currentIndex()==model->index(8,0),"whole-column extension retains original active A9");
 if(name->text()!="6C")std::cerr<<"actual whole-column header: "<<name->text().toStdString()<<'\n';require(name->text()=="6C"&&view->property("worksheet.selectionExtensionActive").toBool(),"six held complete columns expose 6C in the Name Box");
 QTest::keyRelease(view,Qt::Key_Shift);QApplication::processEvents();require(name->text()=="A9"&&!view->property("worksheet.selectionExtensionActive").toBool(),"physical Shift release restores original A9 without changing selected columns");require(view->selectionModel()->selection().front().width()==6,"release preserves the complete A:F selection");
 require(window.currentSession().snapshot()==before,"whole-column selection and release preserve workbook values, styles and history");
}
void wholeColumnHeaderDragShowsHeldDimensions(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");view->setCurrentIndex(model->index(8,2));view->setFocus();const auto before=window.currentSession().snapshot();auto* header=view->horizontalHeader();auto* surface=header->viewport();const auto point=[&](int col){return QPoint(header->sectionViewportPosition(col)+header->sectionSize(col)/2,surface->height()/2);};
 QTest::mousePress(surface,Qt::LeftButton,Qt::NoModifier,point(2));QApplication::processEvents();auto range=view->selectionModel()->selection();require(range.size()==1&&range.front().left()==2&&range.front().width()==1&&range.front().height()==model->rowCount(),"real column-header press selects exactly one complete column");
 if(name->text()!="1C")std::cerr<<"actual pressed column header: "<<name->text().toStdString()<<'\n';require(name->text()=="1C"&&view->property("worksheet.selectionExtensionActive").toBool(),"held column-header press exposes 1C from its actual complete-column range");
 const auto destination=point(7);QMouseEvent move(QEvent::MouseMove,destination,surface->mapToGlobal(destination),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(surface,&move);QApplication::processEvents();range=view->selectionModel()->selection();require(range.size()==1&&range.front().left()==2&&range.front().width()==6&&range.front().height()==model->rowCount(),"real header drag extends exactly six complete columns");require(name->text()=="6C","held header drag exposes actual six-column count");const auto active=view->currentIndex();
 QTest::mouseRelease(surface,Qt::LeftButton,Qt::NoModifier,destination);QApplication::processEvents();require(!view->property("worksheet.selectionExtensionActive").toBool()&&name->text()==QString::fromStdString(columnName(active.column()+1))+QString::number(active.row()+1),"real header release restores active-cell address without a synthetic selection");require(view->selectionModel()->selection().front().width()==6&&window.currentSession().snapshot()==before,"header drag and release preserve selected columns and all workbook/history state");
 const auto send=[&](QEvent::Type type,int col,Qt::MouseButtons buttons){const auto position=point(col);QMouseEvent event(type,position,surface->mapToGlobal(position),Qt::LeftButton,buttons,Qt::NoModifier);QApplication::sendEvent(surface,&event);};send(QEvent::MouseButtonPress,2,Qt::LeftButton);send(QEvent::MouseButtonRelease,2,Qt::NoButton);send(QEvent::MouseButtonPress,3,Qt::LeftButton);QApplication::processEvents();require(name->text()=="1C"&&view->property("worksheet.selectionExtensionActive").toBool(),"a previous release cannot finish a subsequent genuine held header selection");send(QEvent::MouseButtonRelease,3,Qt::NoButton);QApplication::processEvents();require(!view->property("worksheet.selectionExtensionActive").toBool()&&window.currentSession().snapshot()==before,"the subsequent real header release finishes without changing the workbook");
}
void mergedColumnSelectionPreservesActiveCell(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");
 require(model->apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model->sheetId().toStdString()},{"mergedRanges",Json::array({{{"row1",5},{"col1",4},{"row2",5},{"col2",6}}})}}})),"prepare a genuine merged D5:F5 source-shaped cell");view->setCurrentIndex(model->index(4,3));view->setFocus();require(view->columnSpan(4,3)==3,"the native worksheet exposes the real three-column merged span");const auto before=window.currentSession().snapshot();QTest::keyClick(view,Qt::Key_Space,Qt::ControlModifier);auto range=view->selectionModel()->selection();
 if(range.size()==1)std::cerr<<"CtrlSpace merged-column extent: "<<range.front().left()<<","<<range.front().width()<<'\n';require(range.size()==1&&range.front().left()==3&&range.front().width()==3&&range.front().height()==model->rowCount(),"CtrlSpace expands the actual merged-cell width into D:F whole columns");
 for(int n=0;n<3;++n){QTest::keyPress(view,Qt::Key_Left,Qt::ShiftModifier);QTest::keyRelease(view,Qt::Key_Left);}
 range=view->selectionModel()->selection();require(range.size()==1&&range.front().left()==0&&range.front().width()==6&&range.front().height()==model->rowCount(),"held leftward extension grows the merged-column selection to A:F");require(name->text()=="6C"&&view->currentIndex()==model->index(4,3),"six held whole columns preserve the actual D5 active cell");QTest::keyRelease(view,Qt::Key_Shift);QApplication::processEvents();require(name->text()=="D5"&&view->selectionModel()->selection().front().width()==6,"Shift release restores D5 while retaining A:F selection");require(window.currentSession().snapshot()==before,"merged-column selection and extension do not mutate workbook or Undo history");
}
void heldColumnShortcutShowsMergedWidth(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");
 require(model->apply(Json::array({{{"type","set_sheet_layout"},{"sheet",model->sheetId().toStdString()},{"mergedRanges",Json::array({{{"row1",9},{"col1",1},{"row2",9},{"col2",6}}})}}})),"prepare genuine six-column merged A9:F9");view->setCurrentIndex(model->index(8,0));view->setFocus();const auto before=window.currentSession().snapshot();QTest::keyPress(view,Qt::Key_Space,Qt::ControlModifier);QTest::keyRelease(view,Qt::Key_Space);const auto range=view->selectionModel()->selection();require(range.size()==1&&range.front().width()==6&&range.front().height()==model->rowCount(),"held CtrlSpace selects actual whole A:F from the merged span");
 if(name->text()!="6C")std::cerr<<"actual held CtrlSpace header: "<<name->text().toStdString()<<'\n';require(name->text()=="6C"&&view->property("worksheet.selectionExtensionActive").toBool(),"held CtrlSpace exposes merged whole-column count before physical modifier release");QTest::keyRelease(view,Qt::Key_Control);QApplication::processEvents();require(name->text()=="A9"&&!view->property("worksheet.selectionExtensionActive").toBool()&&view->currentIndex()==model->index(8,0),"Control release restores A9 without losing the actual full-column selection");require(view->selectionModel()->selection().front().width()==6&&window.currentSession().snapshot()==before,"held shortcut and release preserve workbook data and history");
 view->setCurrentIndex(model->index(8,8));QTest::keyPress(view,Qt::Key_Space,Qt::ControlModifier);QTest::keyRelease(view,Qt::Key_Space);const auto single=view->selectionModel()->selection();require(single.size()==1&&single.front().left()==8&&single.front().width()==1&&single.front().height()==model->rowCount()&&name->text()=="1C","held CtrlSpace on an unmerged cell exposes genuine single complete-column dimensions");QTest::keyRelease(view,Qt::Key_Control);QApplication::processEvents();require(name->text()=="I9"&&window.currentSession().snapshot()==before,"single-column Control release restores its actual current address and preserves history");
}
void heldSelectionSurvivesWidthPublication(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());
 require(model->setData(model->index(3,6),"7",Qt::EditRole)&&model->setData(model->index(3,7),"8",Qt::EditRole),"prepare independent width targets");view->setCurrentIndex(model->index(3,6));view->setFocus();
 for(int n=0;n<3;++n){QTest::keyPress(view,Qt::Key_Right,Qt::ShiftModifier);QTest::keyRelease(view,Qt::Key_Right);}
 auto* name=window.findChild<QLineEdit*>("formula.name");auto* analysis=window.findChild<QToolButton*>("worksheet.quickAnalysis");require(name->text()=="1R x 4C"&&analysis&&!analysis->isVisible(),"real held Shift selection exposes four-column dimensions and defers Quick Analysis");const auto before=window.currentSession().snapshot();const int original=view->columnWidth(6);
 require(window.executeCommand("columns.size",{{"size",11.14}}),"apply the native width command to the held range");
 require(name->text()=="1R x 4C"&&view->property("worksheet.selectionExtensionActive").toBool()&&!analysis->isVisible(),"layout publication must not impersonate a physical selection release");
 for(int col=6;col<=9;++col)require(view->columnWidth(col)>original,"all four selected columns receive the width");
 require(window.currentSession().snapshotView().at("revision")==before.at("revision").get<int>()+1,"width publication is exactly one workbook transaction");
 QTest::keyRelease(view,Qt::Key_Shift);QApplication::processEvents();require(name->text()=="G4"&&!view->property("worksheet.selectionExtensionActive").toBool()&&analysis->isVisible(),"real Shift release restores the active-cell address and exposes Quick Analysis");
 require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets")&&view->columnWidth(6)==original,"one Undo restores layout and preserves the seeded input");
 require(window.executeCommand("columns.size",{{"size",10.0}})&&name->text()=="G4","a width change after release retains the ordinary cell address");
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 if(qEnvironmentVariable("TIGERSHEETS_DIMENSION_TEST")=="held_column_shortcut"){heldColumnShortcutShowsMergedWidth();std::cout<<"held column shortcut tests passed\n";return 0;}
 if(qEnvironmentVariable("TIGERSHEETS_DIMENSION_TEST")=="merged_columns"){mergedColumnSelectionPreservesActiveCell();std::cout<<"merged whole-column tests passed\n";return 0;}
 if(qEnvironmentVariable("TIGERSHEETS_DIMENSION_TEST")=="header_columns"){wholeColumnHeaderDragShowsHeldDimensions();std::cout<<"whole-column header tests passed\n";return 0;}
 if(qEnvironmentVariable("TIGERSHEETS_DIMENSION_TEST")=="whole_columns"){wholeColumnsShowHeldDimensions();std::cout<<"whole-column selection tests passed\n";return 0;}
 wholeColumnsShowHeldDimensions();
 wholeColumnHeaderDragShowsHeldDimensions();
 mergedColumnSelectionPreservesActiveCell();
 heldColumnShortcutShowsMergedWidth();
 heldSelectionSurvivesWidthPublication();
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());model->setData(model->index(1,1),"Keep",Qt::EditRole);grid->setCurrentIndex(model->index(1,1));grid->selectionModel()->select(QItemSelection(model->index(1,1),model->index(2,2)),QItemSelectionModel::ClearAndSelect);const auto before=window.currentSession().snapshot();
 require(window.executeCommand("rows.size",{{"size",12.0}}),"Row Height edits the selected rows");require(grid->rowHeight(1)==qRound(12.0*grid->logicalDpiY()/72.0)&&grid->rowHeight(2)==grid->rowHeight(1),"row dimensions use points and the selected row count");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"row dimensions are one undoable transaction");require(window.executeCommand("redo"),"row height redo");
 require(window.executeCommand("rows.hide")&&grid->isRowHidden(1)&&grid->isRowHidden(2),"Hide applies to every selected row");require(window.executeCommand("rows.unhide")&&!grid->isRowHidden(1)&&!grid->isRowHidden(2),"Unhide restores the selected rows");
 require(window.executeCommand("columns.size",{{"size",2.0}})&&grid->columnWidth(1)==19,"Column Width uses Excel character units");require(window.executeCommand("columns.hide")&&grid->isColumnHidden(1)&&grid->isColumnHidden(2),"Hide applies to selected columns");require(window.executeCommand("columns.unhide")&&!grid->isColumnHidden(1)&&!grid->isColumnHidden(2)&&model->raw(1,1)=="Keep","Unhide preserves cell values");
 const auto width=grid->columnWidth(1);for(int cycle=0;cycle<2;++cycle){const auto path=state.filePath(QString("dimensions-%1.xlsx").arg(cycle));require(window.saveWorkbook(path)&&window.openWorkbook(path),"dimensions save and reopen");grid=window.findChild<QTableView*>("worksheet.grid");require(grid->columnWidth(1)==width,"character-width authoring retains its pixel width across two native save/reopen cycles");}
 std::cout<<"worksheet dimension action tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
